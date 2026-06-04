#!/usr/bin/env bash
set -e

if [ "$EUID" -ne 0 ]; then
  echo "Error: This script requires root privileges."
  echo "Please run with: sudo bash $0"
  exit 1
fi

echo ">>> Updating package list..."
apt update

echo ">>> Installing build dependencies..."
apt install -y meson ninja-build cmake ragel pkg-config libnuma-dev libpcap-dev libboost-all-dev

echo ">>> Dependencies installation completed."