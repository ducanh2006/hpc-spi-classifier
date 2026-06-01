# High Performance Computing projects overview

**Note:** My primary focus is Project 2

## Overview of the 3 Projects in High Performance Computing

| Project | Category | Project Name | Description | Output Requirements | References |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | High Performance Programming | **HyperDPI: High-Performance Deep Packet Inspection (DPI) using Hyperscan** | Build a DPI system that simulates the mechanism for inspecting and detecting domains/URLs in HTTP/HTTPS traffic using the Hyperscan library | **1. Application requirements:**<br>- Read and compile patterns from configuration file<br>- Classify HTTP/HTTPS traffic<br>- Scan and return MATCH/NO MATCH results<br>- Support multi-pattern matching with regex<br>**2. System documentation**<br>**3. Test cases and throughput benchmark** | - [https://intel.github.io/hyperscan/dev-reference](https://www.google.com/search?q=https://intel.github.io/hyperscan/dev-reference)<br>- RFC 7230 – Hypertext Transfer Protocol (HTTP/1.1)<br>- RFC 8446 – TLS Protocol Version 1.3<br>- DPDK Programmer's Guide |
| 2 | High Performance Programming | **High-Performance SPI Message Classification System** | Build a DPI system that simulates the mechanism for inspecting and detecting domains/URLs in HTTP/HTTPS traffic using the Hyperscan library | **1. Application requirements:**<br>- DPDK-based packet processing application<br>- Classify packets by SPI<br>- Multi-thread pipeline processing<br>**2. System documentation**<br>**3. Test cases and throughput benchmark** | DPDK Programmer's Guide |
| 3 | High Performance Programming | **High-Performance Flow Load Balancing and Flow Table Management System** | Build a flow-based packet load balancing system using DPDK. Packets are hashed by 5-tuple and dispatched to corresponding workers. Each worker manages its own flow table to process packets of the same flow on the same core/thread. | **1. Application requirements:**<br>- DPDK-based flow dispatcher application<br>- Worker flow table management<br>- Multi-core packet processing<br>**2. System documentation**<br>**3. Test cases and throughput benchmark** | DPDK Programmer's Guide |

## Relationship Between the 3 Projects

**Packet Processing Pipeline**
* **Step 1: Receive and Dispatch (Project 3):** Capture raw packets from the network interface card (NIC) using DPDK, group them by flow (5-tuple Hash), and evenly distribute the workload among processing Cores to prevent system overload.
* **Step 2: Decapsulate and Maintain State (Project 2):** Receive the flows from Step 1, track the TCP connection lifecycle (Stateful Packet Inspection), and strip away the network protocol headers (MAC, IP, TCP) to extract the pure application data (L7 Payload).
* **Step 3: Scan and Conclude (Project 1):** Feed the pure L7 Payload from Step 2 into the Hyperscan engine to perform regular expression (Regex) matching, thereby accurately detecting the target domains or URLs.