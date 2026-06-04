#!/bin/bash
# ==============================================================================
# Purpose: Validate if downloaded files are TRUE PCAP/PCAPNG files
# Usage:
#   1. Make execution:  chmod +x scripts/setup_wireshark_samples/verify_pcap.sh
#   2. Run from root:   ./scripts/setup_wireshark_samples/verify_pcap.sh
# ==============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
DATA_DIR="${PROJECT_ROOT}/tests/data"

echo "=================================================="
echo "🔍 STARTING NETWORK TRAFFIC DATA VALIDATION"
echo "=================================================="

# Check if the data directory is empty
if [ -z "$(ls -A "$DATA_DIR")" ]; then
    echo "❌ Error: The directory '$DATA_DIR' is empty! Please run the download script first."
    exit 1
fi

for filepath in "$DATA_DIR"/*; do
    filename=$(basename "$filepath")
    echo "📦 Verifying file: $filename"

    # Step 1: Check Magic Number (File Type)
    # PCAPNG magic bytes: 0a 0d 0d 0a
    # PCAP magic bytes: a1 b2 c3 d4 (or d4 c3 b2 a1)
    file_type=$(file -b "$filepath")
    
    if echo "$file_type" | grep -qiE "pcap capture file|pcapng capture file"; then
        echo "   ✅ Valid format: $file_type"
    else
        echo "   ❌ FORMAT ERROR: This file is diagnosed as '$file_type', NOT a valid PCAP/PCAPNG!"
        echo "--------------------------------------------------"
        continue
    fi

    # Step 2: Deep Inspection - Verify if packets can be parsed successfully
    # Uses capinfos (from wireshark-common/tshark suite) or fallback to tcpdump
    if command -v capinfos &> /dev/null; then
        packet_count=$(capinfos -c "$filepath" | grep "Number of packets" | awk -F: '{print $2}' | tr -d ' ')
        if [ "$packet_count" -gt 0 ]; then
            echo "   ✅ Data integrity OK: File contains $packet_count packets."
        else
            echo "   ⚠️ Warning: File is a valid PCAP container but contains 0 packets."
        fi
    elif command -v tcpdump &> /dev/null; then
        # Fallback to tcpdump if capinfos is missing to read and count parsed packets
        packet_count=$(tcpdump -r "$filepath" -nn -c 10 2>/dev/null | wc -l)
        if [ "$packet_count" -gt 0 ]; then
            echo "   ✅ Data integrity OK: Successfully parsed packets via tcpdump."
        else
             echo "   ⚠️ Warning: File is empty or has a corrupted packet header."
        fi
    else
        echo "   ℹ️ Skipping deep inspection ('tshark' or 'tcpdump' is required to extract packet count)."
    fi
    echo "--------------------------------------------------"
done

echo "🎉 VALIDATION COMPLETED!"