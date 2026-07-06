# **LARGE ASSIGNMENT / MINI PROJECT**

## **SPIFast: High-Performance Shallow Packet Inspection (SPI) System Using DPDK**

**Announced Testing Environment:** Runs directly on a single personal computer (PC/Laptop Linux), simulating the network using the PCAP Virtual Device (vdev) PMD mechanism without relying on physical discrete network cards.  
**Mentor:** Nguyen Ngoc Dung (Email: dungnn11@viettel.com.vn)

# 1. Background

In modern core network systems (Firewalls, UPFs in 5G networks, Load Balancers), traffic classification is a crucial function for applying processing policies and load balancing. The **Shallow Packet Inspection (SPI)** technique allows the system to inspect only L2/L3/L4 header fields (such as Source/Destination IP, Protocol, Source/Destination Port, VLAN ID) to achieve extremely high performance without consuming resources to extract application payloads like DPI.

To accurately simulate and provide the most flexible deployment conditions for students on personal computers (PCs/Laptops) without requiring expensive dedicated network card hardware (Intel X520/X710...), this project utilizes the virtual net_pcap network emulation driver (PCAP Virtual Device PMD) of the DPDK library. This mechanism allows cyclically loading packet capture files (.pcap) directly into the CPU's mbuf buffers, accurately simulating a line-rate traffic flow of up to 1 Gbps.

# 2. Project Objectives

The Mini Project aims to build a simple SPI program using DPDK to perform rule-based packet classification.

The main objectives include:

- Initialize the DPDK environment
- Receive packets from the NIC using DPDK
- Parse Ethernet Header
- Parse IPv4 Header
- Parse TCP/UDP Header
- Extract Five-Tuple information
- Read the rule set from a configuration file
- Perform packet-to-rule matching
- Execute corresponding actions
- Load balance packets to appropriate workers
- Collect and display runtime statistics

Through this Mini Project, students will understand:

- The operating principles of SPI
- How to build a Rule Engine in network systems
- High-speed packet processing mechanisms using DPDK
- Multi-threaded networking system design
- Performance optimization in the data-plane

# 3. Technical Solutions & vdev Simulation Architecture

## 3.1 Virtual Network Replay Mechanism via PCAP vdev

The system does not use a standard physical card driver but instead uses the virtualized virtual network driver 'librte_pmd_pcap'. When the application starts, DPDK will automatically map a capture file 'traffic_sample.pcap' (prepared beforehand using Wireshark) into a logical virtual network interface. Every time the source code calls the `rte_eth_rx_burst()` API function, this virtual driver simulates packet reception by continuously reading byte blocks from the pcap file and loading them directly into the Hugepages memory configuration structure.

## 3.2 Multi-core Network Architecture Thread Layering

To optimize processing performance on a PC, the data path architecture is organized according to a sequential lock-free Pipeline model:

- **Master lcore (Rx / Dispatcher Core):** Runs an infinite loop calling `rte_eth_rx_burst()` to fetch mbufs from the vdev virtual network card. It calls the Parser function to extract the 5-tuple, runs the Rule Matcher to find the Rule ID, and pushes the mbuf pointer into appropriate rings via `rte_ring_enqueue()`.
- **Worker lcores (Worker 0 -> 3):** Run independently on different CPU cores. They continuously poll the queues via `rte_ring_dequeue()` to pull mbufs, update separate processing counters, and release the mbufs back to the Mempool using the `rte_pktmbuf_free()` function.
- **Lock-free Queue (rte_ring):** Acts as an ultra-fast IPC communication bridge between the Master Core and Worker Cores, completely eliminating the use of protection locks (Mutex / Spinlock) - the main cause of network processing performance degradation.

# 4. SPI Rule Specification & Source Code Structure

```c
// Define internal storage structure optimized for Cache-line memory
typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t protocol;
} five_tuple_t;

typedef struct {
    char name[64];
    five_tuple_t tuple;
    uint32_t action_mask;
    uint64_t hit_count; // Bonus: Statistics of rule match count
} spi_rule_t;
```

## 4.1 Sample Configuration File (spi_rules.conf/ .config)

```text
HTTP_TRAFFIC,TCP,*,*,*,80,FORWARD
HTTPS_TRAFFIC,TCP,*,*,*,443,FORWARD
DNS_TRAFFIC,UDP,*,*,*,53,FORWARD
GTPU_TRAFFIC,UDP,*,*,*,2152,FORWARD
SSH_BLOCK,TCP,*,*,*,22,DROP
DEFAULT,*,*,*,*,*,DROP
```

### CRITICAL ARCHITECTURE NOTE:
* **Actions Consist Only of FORWARD or DROP:** Configuration rules will only contain two types of actions: `FORWARD` (forward the packet) or `DROP` (discard the packet). **ABSOLUTELY DO NOT** designate specific workers (such as `FORWARD_WORKER_0`, `FORWARD_WORKER_1`) in the rule file.
* **Workers Have Identical Tasks:** All Worker Cores are completely identical and process **all types** of protocols (HTTP, HTTPS, DNS, SSH, GTPU, etc.). They must not be specialized or hard-coded so that each worker only handles a specific task. Every received packet will be **dynamically load-balanced** by the system among available workers.
* **Dynamic Load Balancing:** To achieve Excellence KPIs (0% drop, 1.48Mpps), you must implement a dynamic packet distribution mechanism (e.g., lock-free Round-Robin, RSS Hash, or Shared Ring) across all active Workers instead of hard-coding rules to specific workers.
* **Zero System Bottlenecks:** Optimize CPU cache efficiency (Cache line) to the maximum and ensure continuous, lock-free mbuf scheduling to eliminate queue congestion (ring congestion).

# 5. Standards & Mandatory Performance Indicators (KPIs)

Although running a simulation on a single PC via the PCAP vdev file loading mechanism, the system must still strictly adhere to the data-plane hardware performance measurement criteria of a 1 Gbps card to ensure students' optimization mindset:

| Performance Parameter | Pass Level | Excellence Level | Measurement Method & Supporting Tools |
| :--- | :--- | :--- | :--- |
| **Throughput** | ≥ 700 Mbps<br>*(When replaying a PCAP file containing packets of average size 512B - 1024B)* | 950 - 990 Mbps<br>*(Approaching the maximum line-rate speed of a 1Gbps network card)* | Measured based on the total number of Bytes received divided by the Delta t time of the runtime clock. |
| **Packet Processing Rate (Flow Rate)** | ≥ 500,000 pps<br>*(0.5 Mpps)* | ≥ 1,488,000 pps<br>*(64B packet line-rate of the 1Gbps Ethernet standard)* | Calculated directly by taking the difference in the number of packets received at the periodic application statistics function. |
| **Packet Drop Rate** | ≤ 0.1%<br>*(at maximum CPU load)* | 0% (Zero Packet Drop)<br>*(Smooth mbuf processing without congestion at the ring)* | Compare the packet drop rate due to overflow of the internal ring buffer queues of the Worker threads. |
| **Missing Packet Rate** | 0% Absolute<br>*(No packets disappear)* | 0% Absolute<br>*(No network counter deviation)* | The total number of packets read from the original PCAP file must exactly match: Total Match + Total Default Drop. |

# 6. Detailed Test Command Execution Guide on Single-PC

## 6.1 Linux System Resource Configuration

Before running the DPDK application, students are required to allocate Hugepages memory resources on the machine:

```bash
# Allocate 1024 pages of 2MB Hugepages (Equivalent to 2GB RAM for PC)
sudo sysctl -w vm.nr_hugepages=1024

# Check successful allocation status
cat /proc/meminfo | grep Huge
```

## 6.2 Application Execution Command for PCAP Loop Replay Simulation

Students prepare a sample file 'traffic_sample.pcap' placed in the running directory. Execute the application command with hard core affinity:

```bash
# Execute the application using core 0 as Rx/Dispatcher, cores 1->4 as Workers
./build/spifast -l 0-4 -n 4 --vdev "net_pcap0,rx_pcap=traffic_sample.pcap,tx_pcap=out_drop.pcap" -- -r spi_rules.conf
```

# 7. Detailed Implementation Process for Students

- **Step 1:** Configure Hugepages on Ubuntu Linux/personal PC.
- **Step 2:** Use Wireshark to capture a traffic_sample.pcap file containing a mix of HTTP (port 80), HTTPS (port 443), DNS (port 53), and SSH (port 22) packets to prepare the network input dataset.
- **Step 3:** Write C source code to initialize the DPDK EAL environment, create a Mempool buffer of moderate size suitable for a PC (e.g., 8192 or 16384 mbufs).
- **Step 4:** Implement the Rule Parser file structure to read the spi_rules.conf configuration file.
- **Step 5:** Develop the Header Parser extraction logic (Zero-copy casting of mbuf pointers to existing DPDK network header structs) and the First Match algorithm.
- **Step 6:** Create multi-threaded multi-core linkages using the `rte_eal_remote_launch()` function combined with passing mbufs via `rte_ring`.
- **Step 7:** Write the Statistics function to periodically output converted Mbps, pps values, and hit counters of each network rule to the console screen every 1 second for result acceptance.

# 8. Evaluation Criteria & Required Deliverables

- Standard C source code (Including Makefile/CMakeLists.txt configuration files) that can compile cleanly without errors (Warnings/Errors) in a GCC Linux environment.
- Testcase file (Excel): Function test & performance test.
- System analysis documentation clearly explaining the used DPDK API functions and the packet routing mapping diagram between CPU lcores.
- Statistical table of actual run results on the machine printed from the application, clearly demonstrating that the system achieves the data classification KPIs (pps, Mbps) corresponding to the 1 Gbps network simulation environment.

# 9. Real-time Configuration Reload Feature

The project requires support for updating configuration rules while the application is running **without needing to stop or restart**. There are 2 implementation options:

## 9.1 Option 1: Basic Approach

Edit the rules directly on the text configuration file (`.conf` or `.txt`), then write a command-line tool (CLI) to invoke the `reload runtime` command to update the rules into the running system:

```bash
# Example CLI command to reload configuration
./spi_cli reload_rules spi_rules.conf
```

**Advantages:** Simple, easy to implement, suitable for a mini project.  
**Disadvantages:** Requires an IPC mechanism between the CLI tool and the main application (can use Unix socket or named pipe).

## 9.2 Option 2: Advanced Approach

Use 3rd party configuration tools like **Netconf** or **confd** to manage and update configurations in real-time.

**Advantages:** Industry standard, supports configuration versioning, automatic change monitoring.  
**Disadvantages:** More complex, requires installation of 3rd party tools.

**Note:** This project encourages the use of **Option 1** to maintain simplicity and focus on the core of SPI classification. Option 2 is considered an Advanced Feature if there is time and a desire to explore further.