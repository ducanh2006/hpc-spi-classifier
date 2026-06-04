#!/usr/bin/env bash

echo ">>> Verifying dependencies..."

check_tool() {
    local tool_name=$1
    local version_cmd=$2
    
    if command -v "$tool_name" &> /dev/null; then
        local version
        if [ -n "$version_cmd" ]; then
            version=$(eval "$version_cmd")
        else
            version="installed"
        fi
        echo " - [OK] $tool_name: $version"
        return 0
    else
        echo " - [ERROR] $tool_name is missing!"
        return 1
    fi
}

MISSING=0
check_tool "gcc" "gcc --version | head -n 1" || MISSING=$((MISSING+1))
check_tool "meson" "meson --version" || MISSING=$((MISSING+1))
check_tool "ninja" "ninja --version" || MISSING=$((MISSING+1))
check_tool "cmake" "cmake --version | head -n 1" || MISSING=$((MISSING+1))
check_tool "ragel" "ragel -v | head -n 1" || MISSING=$((MISSING+1))
check_tool "pkg-config" "pkg-config --version" || MISSING=$((MISSING+1))

if [ "$MISSING" -gt 0 ]; then
    echo ">>> $MISSING tools are missing. Please run install_dependencies.sh."
    exit 1
fi

echo ">>> All dependencies are ready!"