# ==============================================================================
# Purpose: Download Wireshark PCAP samples for DPI/SPI testing
# Usage:
#   1. Make execution:  chmod +x tests/scripts/download_samples.sh
#   2. Run from root:   ./tests/scripts/download_samples.sh
# ==============================================================================

set -euo pipefail  

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
DATA_DIR="${PROJECT_ROOT}/tests/data"

BASE_URL="https://wiki.wireshark.org/SampleCaptures?action=AttachFile&do=get&target="

declare -A SAMPLE_FILES=( 
    ["http.cap"]=""                                 
    ["tls13-93-www-google-com.pcapng"]=""              
    ["custom.pcap"]="https://other-source.com/file.cap" 
)

mkdir -p "${DATA_DIR}"

for filename in "${!SAMPLE_FILES[@]}"; do
    output="${DATA_DIR}/${filename}"
    custom_url="${SAMPLE_FILES[$filename]}"
    
    [[ -f "${output}" ]] && { echo "✅ Skip: ${filename}"; continue; }
    
    url="${custom_url:-${BASE_URL}${filename}}"  
    echo "⬇️  Downloading: ${filename}..."
    
    if curl -L -s --retry 3 --connect-timeout 10 -o "${output}" "${url}"; then
        echo "✅ Success: ${filename}"
    else
        echo "❌ Failed: ${filename}" >&2
        rm -f "${output}"
        exit 1  
    fi
done

echo "🎉 Done! Test data ready in '${DATA_DIR}'"
