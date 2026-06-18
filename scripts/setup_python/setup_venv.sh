#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VENV_DIR="$PROJECT_ROOT/venv"
REQUIREMENTS="$SCRIPT_DIR/requirements.txt"

echo ">>> Setting up Python Virtual Environment..."

# Ensure python3-venv is installed on the system
if ! python3 -c "import venv" &> /dev/null; then
    echo "[ERROR] python3-venv is not installed on your system."
    echo "Please install it first: sudo apt install python3-venv"
    exit 1
fi

if [ ! -d "$VENV_DIR" ]; then
    echo " - Creating virtual environment at $VENV_DIR"
    python3 -m venv "$VENV_DIR"
else
    echo " - Virtual environment already exists at $VENV_DIR"
fi

echo " - Activating virtual environment..."
source "$VENV_DIR/bin/activate"

echo " - Upgrading pip..."
pip install --upgrade pip

if [ -f "$REQUIREMENTS" ]; then
    echo " - Installing dependencies from $REQUIREMENTS..."
    pip install -r "$REQUIREMENTS"
else
    echo " - No requirements.txt found. Skipping dependency installation."
fi

echo "=================================================="
echo "🎉 Python Virtual Environment setup complete!"
echo "=================================================="
echo "To activate it in your current terminal session, run:"
echo "source venv/bin/activate"
echo "=================================================="
