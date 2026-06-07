#!/bin/bash
# ==============================================================================
# Purpose: Download Wireshark PCAP samples for DPI/SPI testing
# Usage:
#   1. Make execution:  chmod +x scripts/setup_wireshark_samples/download_wireshark_samples.sh
#   2. Run from root:   ./scripts/setup_wireshark_samples/download_wireshark_samples.sh
# ==============================================================================

set -euo pipefail  

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
DATA_DIR="${PROJECT_ROOT}/tests/data/pcap"

# Using raw links from Wireshark's official GitLab repository
GITLAB_RAW_BASE="https://gitlab.com/wireshark/wireshark/-/raw/master/test/captures/"

# Exact filenames as they exist in the Wireshark test repository
declare -A SAMPLE_FILES=(
    ["http.pcap"]=""
    ["tls13-rfc8446.pcap"]=""
)

mkdir -p "${DATA_DIR}"

for filename in "${!SAMPLE_FILES[@]}"; do
    output="${DATA_DIR}/${filename}"
    custom_url="${SAMPLE_FILES[$filename]}"
    
    # Check if file already exists and its size is greater than 0 bytes to skip
    if [[ -f "${output}" ]] && [[ -s "${output}" ]]; then
        echo "✅ Skip: ${filename} (already exists)"
        continue
    fi

    url="${custom_url:-${GITLAB_RAW_BASE}${filename}}"  
    echo "⬇️  Downloading: ${filename}..."

    # Added -f flag to force curl to fail silently if HTTP code is >= 400
    if curl -L -s --retry 3 --connect-timeout 10 -f -o "${output}" "${url}"; then
        # Prevent false positives where an HTML error page is downloaded instead of binary
        if head -c 5 "${output}" | grep -q "<!DOC"; then
            echo "❌ Error: ${filename} downloaded as HTML (invalid URL or file moved)"
            rm -f "${output}"
            exit 1
        fi
        echo "✅ Success: ${filename}"
    else
        echo "❌ Failed: ${filename} (HTTP error or network issue)" >&2
        rm -f "${output}"
        exit 1  
    fi
done

echo "🎉 Done! Test data ready in '${DATA_DIR}'"