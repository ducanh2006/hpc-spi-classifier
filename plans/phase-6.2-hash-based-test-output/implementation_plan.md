# Implement Debug Mode for Functional Testing

This plan details the steps to introduce a functional testing debug mode for the SPIFast application without impacting the production build.

## Proposed Changes

### Build System

#### [MODIFY] [meson.build](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/meson.build)
- Add a new executable target `spifast_debug`.
- Compile it with the same sources but add `-DDEBUG_MODE` to `c_args`.

### Core Application Logic (C Code)

#### [MODIFY] [src/common.h](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/common.h)
- Introduce a global `volatile bool force_quit` to signal all threads to gracefully terminate.
- In `pkt_metadata_t`, conditionally add `uint64_t packet_index;` when `DEBUG_MODE` is defined.

#### [MODIFY] [src/main.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/main.c)
- Define `volatile bool force_quit = false;`.
- Add a signal handler for `SIGINT` / `SIGTERM` to set `force_quit = true`.

#### [MODIFY] [src/master.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.c)
- Loop condition will check `!force_quit`.
- In `DEBUG_MODE`:
  - Skip calling `stats_print_periodic()`.
  - Maintain a `static uint64_t debug_packet_idx = 0;`.
  - Assign `meta->packet_index = debug_packet_idx++;` for each received packet.
  - Detect the end of the pcap stream by counting `idle_loops` when `nb_rx == 0`. If `idle_loops` exceeds a threshold (e.g., 5,000,000) and `debug_packet_idx > 0`, set `force_quit = true` and exit the `master_loop` to automatically terminate the application.

#### [MODIFY] [src/worker.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c)
- Loop condition will check `!force_quit`.
- In `DEBUG_MODE`:
  - Open `tests/results/actual.csv` for writing (only for `worker_id == 0`).
  - Write the CSV header: `Packet_Index,Rule,Action\n`.
  - Right after matching a rule, use `fprintf` to log `packet_index`, `rule_name` (or `DEFAULT` / `INVALID`), and the outcome action.
  - Before the worker thread exits, properly close the file (`fclose()`).

### Test Generation & Verification

#### [NEW] [tests/gen_tests/gen_func_test.py](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/tests/gen_tests/gen_func_test.py)
- Read `spi_rules.conf` to discover defined rules.
- Generate a smaller sequence of packets covering:
  - Exact matches for each rule.
  - A no-match packet (e.g., protocol or port mismatch) which should trigger `DEFAULT` drop or unhandled drop.
  - An invalid packet (e.g., non-IP payload) to test the parser rejection.
- Save the pcap to `tests/data/pcap/func_test.pcap`.
- Generate the expected outputs into `tests/data/csv/func_test_map.csv`.

## Verification Plan

### Automated Tests
1. Generate the test dataset:
   `python tests/gen_tests/gen_func_test.py`
2. Build the debug binary:
   `meson compile -C build spifast_debug`
3. Run the debug binary with the virtual PCAP PMD using 1 worker:
   `./build/spifast_debug -l 0-1 --vdev=net_pcap0,rx_pcap=tests/data/pcap/func_test.pcap -- -r spi_rules.conf`
4. Evaluate correctness:
   `python tests/judge/check_correctness.py tests/data/csv/func_test_map.csv tests/results/actual.csv`

### Manual Verification
- Verify that `stats_print_periodic()` does not clutter the logs in debug mode.
- Ensure the program terminates automatically when the PCAP is fully processed.
