# Debug Mode Walkthrough

The functional testing debug mode has been successfully implemented and integrated into the SPIFast project. This allows testing with deterministic inputs and exact matching of rule classification logic.

## Changes Implemented

1. **Build System Updates**
   - Added `spifast_debug` as a new executable target in [meson.build](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/meson.build), compiled with the `-DDEBUG_MODE` flag.

2. **Core Source Code Hooks**
   - Added `force_quit` variable and signal handlers to gracefully shut down the application in [main.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/main.c).
   - In [master.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.c), disabled periodic stats, assigned `packet_index` to packets during debugging, and added logic to automatically trigger exit when `net_pcap0` completes reading the file (after a threshold of idle loops).
   - In [worker.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c), `Worker 0` initializes `tests/results/actual.csv`. Right after parsing and classifying a packet, the packet index, matched rule, and action are explicitly written to the output file.

3. **Test Data Generator**
   - Implemented [gen_func_test.py](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/tests/gen_tests/gen_func_test.py) which dynamically reads `spi_rules.conf` and creates packets matching every rule, plus one `DEFAULT` (no match) and one `INVALID` packet. It saves both the virtual PCAP and expected baseline.

4. **Execution Convenience**
   - Created [run_func_test.sh](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/tests/judge/run_func_test.sh) that acts similarly to the native benchmarking script but points to the debug executable, test PMDs, and runs the entire suite (from Python generation to C execution to Python verification) automatically.

## Running the Verification

Since DPDK requires elevated privileges and correctly loading PMD drivers securely from an isolated directory, I have created a wrapper to automate this logic for you. 

You can execute the entire pipeline by running the following command in your terminal:
```bash
sudo ./tests/judge/run_func_test.sh
```

This will:
1. Generate fresh `func_test.pcap` and `func_test_map.csv`
2. Compile `spifast_debug`
3. Launch DPDK with virtual device integration using 1 worker
4. Evaluate the actual outcomes using `check_correctness.py`

Please go ahead and run this script to ensure `actual.csv` accurately aligns with `func_test_map.csv`. Let me know if you need any adjustments or run into any PMD environment issues.
