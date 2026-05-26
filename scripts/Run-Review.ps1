param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [int]$AgyTimeoutSeconds = 900,

    [ValidateSet("All", "Codex", "Agy", "Claude", "Consensus")]
    [string]$Mode = "All"
)

$ErrorActionPreference = "Stop"
$repoRoot = [System.IO.Path]::GetFullPath($RepoRoot)
$agyTimeoutSeconds = $AgyTimeoutSeconds

$reviewPromptPath = Join-Path $repoRoot "plans\prompts\review.md"
$consensusPromptPath = Join-Path $repoRoot "plans\prompts\consensus_review.md"
$reviewsDir = Join-Path $repoRoot "plans\reviews"
$codexReviewPath = Join-Path $reviewsDir "review-codex.md"
$geminiReviewPath = Join-Path $reviewsDir "review-gemini.md"
$claudeReviewPath = Join-Path $reviewsDir "review-claude.md"
$consensusReviewPath = Join-Path $reviewsDir "review-consensus.md"
$codexRunLogPath = Join-Path $reviewsDir "review-codex-run.log"
$geminiRunLogPath = Join-Path $reviewsDir "review-gemini-agy.log"
$geminiStdIoLogPath = Join-Path $reviewsDir "review-gemini-stdio.log"
$claudeRunJsonPath = Join-Path $reviewsDir "review-claude-run.json"
$consensusCodexMessagePath = Join-Path $reviewsDir "review-consensus-codex-message.md"
$consensusRunLogPath = Join-Path $reviewsDir "review-consensus-codex-run.log"

Set-Location $repoRoot
New-Item -ItemType Directory -Force -Path $reviewsDir, "kanban\ice-box", "kanban\pending", "kanban\done" | Out-Null

function Assert-PathExists {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description not found: $Path"
    }
}

function Assert-CommandAvailable {
    param([string]$CommandName, [string]$InstallHint)
    $command = Get-Command $CommandName -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Could not find $CommandName on PATH. $InstallHint"
    }
}

function Resolve-CodexCommand {
    $command = Get-Command codex.cmd -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $command = Get-Command codex.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $application = Get-Command codex -All -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandType -eq "Application" } |
        Select-Object -First 1
    if ($application) {
        return $application.Source
    }

    $shim = Get-Command codex -ErrorAction SilentlyContinue
    if ($shim) {
        throw "Found $($shim.Source), but review needs codex.cmd or codex.exe to avoid PowerShell stderr shim failures."
    }

    throw "Could not find codex.cmd or codex.exe on PATH. Install or add the Codex CLI to PATH."
}

function Resolve-AgyCommand {
    $command = Get-Command agy -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $fallback = Join-Path $env:LOCALAPPDATA "agy\bin\agy.exe"
    if (Test-Path -LiteralPath $fallback) {
        return $fallback
    }

    throw "Could not find agy on PATH or at $fallback."
}

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory = $true)]
        [scriptblock]$ScriptBlock,

        [string]$LogPath
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & $ScriptBlock 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    $outputLines = @($output | ForEach-Object { [string]$_ })
    if ($LogPath) {
        $outputLines | Set-Content -Encoding utf8 -LiteralPath $LogPath
    }
    $outputLines | ForEach-Object { Write-Host $_ }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = $outputLines
    }
}

function Assert-MeaningfulReview {
    param([string]$Path, [string]$Name)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Name review did not produce $Path"
    }

    $text = Get-Content -Raw -LiteralPath $Path
    if ([string]::IsNullOrWhiteSpace($text)) {
        throw "$Name review output is empty: $Path"
    }

    if ($text.Trim().Length -lt 500) {
        throw "$Name review output is too short to be a useful review: $Path"
    }

    $failurePatterns = @(
        "CreateProcessAsUserW failed: 1312",
        "Agy produced no stdout",
        "You are not logged into Antigravity",
        "failed to get model config",
        "failed to get load code assist response",
        "Failed to get OAuth token",
        "permission denied",
        "not recognized as"
    )

    foreach ($pattern in $failurePatterns) {
        if ($text -match [regex]::Escape($pattern)) {
            throw "$Name review output looks like a failure ($pattern): $Path"
        }
    }
}

function Get-CleanAgyOutput {
    param([object[]]$Output)
    return @($Output |
        ForEach-Object { [string]$_ } |
        Where-Object {
            $_ -and
            $_.Trim() -ne "YOLO mode is enabled. All tool calls will be automatically approved." -and
            $_.Trim() -ne "Loaded cached credentials."
        })
}

function Run-CodexReview {
    Write-Host "Running Codex review..."
    $reviewPrompt = Get-Content -Raw -LiteralPath $reviewPromptPath
    $codexReviewPrompt = @"
$reviewPrompt

Host instruction: perform review only. Do not edit repository files. Return the full review as markdown.
"@

    $result = Invoke-NativeCommand -LogPath $codexRunLogPath -ScriptBlock {
        $codexReviewPrompt |
            & $codexCommand exec --skip-git-repo-check -C $repoRoot --sandbox danger-full-access -o $codexReviewPath -
    }
    $exitCode = $result.ExitCode
    if ($exitCode -ne 0) {
        throw "Codex review failed with exit code $exitCode. See $codexRunLogPath"
    }

    Assert-MeaningfulReview $codexReviewPath "Codex"
}

function Run-GeminiReview {
    Write-Host "Running Gemini review through agy..."
    $agyCommand = Resolve-AgyCommand
    if (Test-Path -LiteralPath $geminiRunLogPath) {
        Remove-Item -LiteralPath $geminiRunLogPath -Force
    }
    if (Test-Path -LiteralPath $geminiStdIoLogPath) {
        Remove-Item -LiteralPath $geminiStdIoLogPath -Force
    }
    if (Test-Path -LiteralPath $geminiReviewPath) {
        Remove-Item -LiteralPath $geminiReviewPath -Force
    }

    $reviewPrompt = Get-Content -Raw -LiteralPath $reviewPromptPath
    $agyPrompt = @"
$reviewPrompt

Host instruction: perform review only. Write the complete review to this exact file path:
$geminiReviewPath

Also print the same markdown to stdout. Do not modify any other repository files.
"@
    $printTimeout = "$($agyTimeoutSeconds)s"
    $result = Invoke-NativeCommand -LogPath $geminiStdIoLogPath -ScriptBlock {
        & $agyCommand --log-file $geminiRunLogPath --print $agyPrompt --print-timeout $printTimeout --dangerously-skip-permissions
    }
    $exitCode = $result.ExitCode
    $agyOutput = $result.Output
    $cleanOutput = Get-CleanAgyOutput $agyOutput

    if ($exitCode -ne 0) {
        $agyOutputText = ($agyOutput | Out-String).Trim()
        $failureText = @"
# Gemini review failed

Agy exited with code $exitCode.

Stdio log: $geminiStdIoLogPath
Agy log: $geminiRunLogPath

Output:

$agyOutputText
"@
        $failureText | Set-Content -Encoding utf8 $geminiReviewPath
        throw "Gemini review failed with exit code $exitCode. See $geminiReviewPath"
    }

    if (Test-Path -LiteralPath $geminiReviewPath) {
        try {
            Assert-MeaningfulReview $geminiReviewPath "Gemini"
            return
        } catch {
            Write-Host "Agy wrote $geminiReviewPath, but it did not validate: $($_.Exception.Message)"
        }
    }

    if (-not $cleanOutput) {
        $logText = if (Test-Path -LiteralPath $geminiRunLogPath) { Get-Content -Raw -LiteralPath $geminiRunLogPath } else { "" }
        $logTail = if ($logText) { (($logText -split '\r?\n') | Select-Object -Last 80) -join "`n" } else { "<no agy log was written>" }
        $failureText = @"
# Gemini review failed

Agy produced no stdout.

Stdio log: $geminiStdIoLogPath
Agy log: $geminiRunLogPath

Log tail:

$logTail
"@
        $failureText | Set-Content -Encoding utf8 $geminiReviewPath
        throw "Gemini review failed because agy produced no stdout. See $geminiReviewPath"
    }

    $cleanOutput | Set-Content -Encoding utf8 $geminiReviewPath
    Assert-MeaningfulReview $geminiReviewPath "Gemini"
}

function Run-ClaudeReview {
    Write-Host "Running Claude review..."
    $reviewPrompt = Get-Content -Raw -LiteralPath $reviewPromptPath
    $claudePrompt = @"
$reviewPrompt

Host instruction: perform review only. Do not edit repository files. Return the full review as markdown in your final result.
"@

    $claudeJson = $claudePrompt |
        claude -p --output-format json --permission-mode bypassPermissions --allowedTools Read Grep Glob Bash |
        Out-String
    $exitCode = $LASTEXITCODE
    $claudeJson | Set-Content -Encoding utf8 $claudeRunJsonPath
    if ($exitCode -ne 0) {
        throw "Claude review failed with exit code $exitCode. See $claudeRunJsonPath"
    }

    $claudeResult = [string](($claudeJson | ConvertFrom-Json).result)
    $claudePlanPath = [regex]::Match($claudeResult, '[A-Za-z]:\\[^\r\n"`]*?\\.claude\\plans\\[^\r\n"`]+?\.md')
    if ($claudePlanPath.Success -and (Test-Path -LiteralPath $claudePlanPath.Value)) {
        Copy-Item -LiteralPath $claudePlanPath.Value -Destination $claudeReviewPath
    } else {
        $claudeResult | Set-Content -Encoding utf8 $claudeReviewPath
    }

    Assert-MeaningfulReview $claudeReviewPath "Claude"
}

function Run-ConsensusReview {
    Write-Host "Running consensus review..."
    $consensusPrompt = Get-Content -Raw -LiteralPath $consensusPromptPath
    $result = Invoke-NativeCommand -LogPath $consensusRunLogPath -ScriptBlock {
        $consensusPrompt |
            & $codexCommand exec --skip-git-repo-check -C $repoRoot --sandbox danger-full-access -o $consensusCodexMessagePath -
    }
    $exitCode = $result.ExitCode
    if ($exitCode -ne 0) {
        throw "Consensus review failed with exit code $exitCode. See $consensusRunLogPath"
    }

    Assert-MeaningfulReview $consensusReviewPath "Consensus"
}

Assert-PathExists $reviewPromptPath "Review prompt"
Assert-PathExists $consensusPromptPath "Consensus prompt"

if ($Mode -in @("All", "Codex", "Consensus")) {
    $codexCommand = Resolve-CodexCommand
}
if ($Mode -in @("All", "Claude")) {
    Assert-CommandAvailable "claude" "Install or add the Claude CLI to PATH."
}
if ($Mode -in @("All", "Agy")) {
    Resolve-AgyCommand | Out-Null
}

switch ($Mode) {
    "All" {
        Run-CodexReview
        Run-GeminiReview
        Run-ClaudeReview
        Write-Host "All reviews succeeded; running consensus..."
        Run-ConsensusReview
    }
    "Codex" {
        Run-CodexReview
    }
    "Agy" {
        Run-GeminiReview
    }
    "Claude" {
        Run-ClaudeReview
    }
    "Consensus" {
        Run-ConsensusReview
    }
}
