# Walkthrough - Implement Packet Drop Rate and Missing Packet Rate Statistics

I have successfully added the packet drop rate and missing packet rate calculations to the periodic statistics print function.

## Changes Made

### Statistics Engine
- **[stats.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/stats.c)**:
  - Added calculations for `drop_rate` and `missing_rate`.
  - Displayed `Packet Drop Rate` and `Missing Packet Rate` alongside other throughput and processing metrics in `stats_print_periodic()`.

## Verification Details

### Compilation
The codebase compiles cleanly with:
```bash
ninja -C build
```
No errors or warnings were introduced.
