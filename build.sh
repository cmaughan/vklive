#!/bin/bash
set -euo pipefail

CONFIG="${1:-debug}"
python3 do.py build "${CONFIG}"
