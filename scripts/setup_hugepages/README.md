# Hugepages Setup Guide

This directory contains utility scripts to allocate and verify **Hugepages** for the SPIFast DPDK application.

##  Files

- **`setup_hugepages.sh`**: Allocates 1024 pages of 2MB (2GB total) and mounts `hugetlbfs`.
- **`verify_setup_hugepages.sh`**: Inspects `/proc/meminfo` and active mount tables to verify allocation.

##  How to Use

### 1. Configure Hugepages
Run the setup script with root privileges (`sudo`):
```bash
chmod +x scripts/setup_hugepages/setup_hugepages.sh
sudo ./scripts/setup_hugepages/setup_hugepages.sh
```

### 2. Verify Configuration
Verify that the allocation and mount points are active:
```bash
chmod +x scripts/setup_hugepages/verify_setup_hugepages.sh
./scripts/setup_hugepages/verify_setup_hugepages.sh
```
