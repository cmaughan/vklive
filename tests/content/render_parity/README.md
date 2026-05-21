# Render Parity Fixtures

This directory is reserved for render parity harness fixtures that need local test data. The initial harness runs existing sample projects from `run_tree/projects` and launches `Rezonality` with `--startup-frame-test` so each renderer can initialize, draw one frame, and exit under CTest.
