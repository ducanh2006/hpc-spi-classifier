# Wireshark Samples Setup Guide

This directory contains utility scripts to download and verify official Wireshark sample PCAP files used for SPI testing.

## Files

- **`download_wireshark_samples.sh`**: Downloads sample network packet captures (e.g., HTTP and TLS 1.3) from the official Wireshark repository into the `tests/data/` folder.
- **`verify_pcap.sh`**: Inspects the downloaded files to ensure they are valid PCAP/PCAPNG formats and contain packets.

## How to Use

### 1. Download Samples
Run the download script from the project root:
```bash
chmod +x scripts/setup_wireshark_samples/download_wireshark_samples.sh
./scripts/setup_wireshark_samples/download_wireshark_samples.sh
```

### 2. Verify Samples
Run the verification script to check file integrity:
```bash
chmod +x scripts/setup_wireshark_samples/verify_pcap.sh
./scripts/setup_wireshark_samples/verify_pcap.sh
```
