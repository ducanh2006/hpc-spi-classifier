# Implementation Plan - Add Drop Rate and Missing Packet Rate Statistics

Implement the missing data-plane KPIs: Packet Drop Rate and Missing Packet Rate. These will be added to the periodic statistics print function to satisfy system verification requirements.

## Proposed Changes

### Statistics Component

#### [MODIFY] [stats.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/stats.c)

- Calculate and print `Packet Drop Rate (%)` and `Missing Packet Rate (%)` inside `stats_print_periodic()`.
  - **Packet Drop Rate**: Represents the percentage of packets dropped at the ring queues due to overflow:
    $$\text{Packet Drop Rate} = \frac{g\_master\_dropped\_packets}{g\_master\_rx\_packets} \times 100\%$$
  - **Missing Packet Rate**: Measures packets read from the virtual device but unaccounted for in worker processing or enqueue drop counters:
    $$\text{Missing Packets} = g\_master\_rx\_packets - (g\_master\_dropped\_packets + worker\_rx\_pkts)$$
    $$\text{Missing Packet Rate} = \frac{missing\_packets}{g\_master\_rx\_packets} \times 100\%$$

## Verification Plan

### Automated/Manual Verification
- Compile the code using:
  ```bash
  ninja -C build
  ```
- Run the simulation with a sample PCAP file and verify the printed output format.
