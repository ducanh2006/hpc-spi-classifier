#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VENV_DIR="$PROJECT_ROOT/venv"

# Nếu có thư mục venv, sử dụng python của venv
PYTHON_BIN="python3"
if [ -d "$VENV_DIR" ]; then
    PYTHON_BIN="$VENV_DIR/bin/python"
fi

if [ "$#" -lt 2 ]; then
    echo "Usage: ./tests/judge/run_check_correctness.sh <expected_csv> <actual_csv>"
    echo "Example: ./tests/judge/run_check_correctness.sh tests/data/csv/balanced_traffic_map.csv tests/results/balanced_traffic_actual.csv"
    exit 1
fi

EXPECTED_CSV="$1"
ACTUAL_CSV="$2"

echo "===================================================="
echo "             SPIFast Correctness Checker            "
echo "===================================================="
echo " - Python    : $PYTHON_BIN"
echo " - Expected  : $EXPECTED_CSV"
echo " - Actual    : $ACTUAL_CSV"
echo "----------------------------------------------------"

$PYTHON_BIN "$SCRIPT_DIR/check_correctness.py" "$EXPECTED_CSV" "$ACTUAL_CSV"

echo "===================================================="
