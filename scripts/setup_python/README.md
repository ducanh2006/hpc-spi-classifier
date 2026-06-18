# Python Environment Setup Guide

This directory contains utility scripts to set up an isolated Python Virtual Environment (`venv`) for the SPIFast project. This is highly recommended for managing Python dependencies like `pyelftools` (required by DPDK) without polluting or conflicting with your system-wide Python installation.

## Files

- **`setup_venv.sh`**: Creates a `venv/` directory at the project root, activates it, and installs necessary Python packages via pip.
- **`requirements.txt`**: A list of required Python dependencies (e.g., `pyelftools`, `meson`, `ninja`).

## How to Use

### 1. Run the Setup Script
Execute the script to automatically create and configure the virtual environment:
```bash
chmod +x scripts/setup_python/setup_venv.sh
./scripts/setup_python/setup_venv.sh
```

### 2. Activate the Environment
Before compiling DPDK or running the project's build system, you **must activate** the virtual environment in your current terminal session:
```bash
source venv/bin/activate
```
*(You will see `(venv)` prepended to your command prompt, indicating that the virtual environment is active).*

### 3. Deactivate (Optional)
When you are done and want to return to the system Python environment, simply run:
```bash
deactivate
```

## Note on `.gitignore`
The project's `.gitignore` explicitly ignores the `venv/` directory (`venv/*`), so the isolated environment and its installed libraries will never be committed to Git.
