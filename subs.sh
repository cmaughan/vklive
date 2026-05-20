#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$repo_root"

git pull

git -C libs/zing checkout main
git -C libs/zing pull

git -C libs/zing/libs/zest checkout main
git -C libs/zing/libs/zest pull

git -C zep checkout master
git -C zep pull
