# COL7524: Advanced Computer Networks

This directory contains assignments and simulation models for **COL7524: Advanced Computer Networks**, focusing on low-level network topology design, protocol implementations, and network performance evaluation using the **Network Simulator 3 (ns-3)** framework.

---

## 📁 Course Structure

* **[ASSIGNMENT_1](./ASSIGNMENT_1/)**
  * **Objective:** Implement a BGP-lite style network simulation using C++ in ns-3.
  * **Topology:** 3 routers connected in a line (`10.0.1.0/24` and `10.0.2.0/24`), routing traffic from a source UDP client to a destination UDP echo server.
  * **Structure:**
    * `Part0` to `PartE`: Incremental steps for compiling, executing, and graphing simulation results.
    * `bgp.cc`: The main simulation source file using ns-3 helper modules (`PointToPointHelper`, `Ipv4GlobalRoutingHelper`, `FlowMonitorHelper`).
    * `bgp-lite-results.xml`: Serialized flow statistics showing packet delivery, throughput, and delays.
    * `COL724_Asignment_1-1.pdf`: Detailed project assignment specifications.
* **[ASSIGNMENT_2](./ASSIGNMENT_2/)**
  * **Objective:** Advanced protocol testing and network analysis.
  * **Structure:** Incremental stages across `PART_A`, `PART_B`, and `PART_C`.
  * **Documentation:** `COL724_Asignment2.pdf` provides the official assignment specifications.

---

## ⚙️ Compilation & Execution (ns-3)

To run the simulations inside a configured ns-3 environment:

1. **Copy the script** to the ns-3 scratch directory:
   ```bash
   cp ASSIGNMENT_1/bgp.cc /path/to/ns3/scratch/bgp.cc
   ```

2. **Run the simulation**:
   ```bash
   ./ns3 run scratch/bgp
   ```

3. **Check results**:
   The simulation outputs a `bgp-lite-results.xml` file. You can open and parse it using standard XML parsers or graph results to analyze packet delay, loss, and channel throughput.

---
[← Semester 1 Index](../README.md) | [← Portfolio Root](../../README.md)
