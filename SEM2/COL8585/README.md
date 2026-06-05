# COL8585: System and Network Security

This directory contains programming assignments and lab modules for **COL8585: System and Network Security** at IIT Delhi. The coursework covers kernel-level network packet filtering, host protection, and cryptographic VPN traffic dissection.

---

## 📁 Course Structure

### 🛡️ [ASSIGNMENT 1: Platinum Shield Kernel Firewall](./ASSIGNMENT_1/)
* **Objective:** Implement a kernel-level HTTP packet-filtering module in C.
* **Details:**
  * Uses the **FreeBSD pfil** packet filtering framework (`net/pfil.h`).
  * Intercepts incoming IPv4 TCP traffic (`PFIL_IN`) on port 80.
  * Inspects TCP socket data buffers (`mbuf`) and performs string matching to check for prohibited domain requests (e.g., `"blocked.com"`).
  * Automatically drops the matching packets (`m_freem`), increments statistics, and prints system logs (`SHIELD_BLOCK`) displaying dropped packet counts and payload sizes.
* **Specifications:** Detailed requirements are documented in `COL8585_ASSIGN_1_P1.pdf` and `COL8585_ASSIGN_1_P2.pdf`.

### 🔍 [ASSIGNMENT 2: IronDome VPN Traffic Analyzer](./ASSIGNMENT_2/)
* **Objective:** Build port-independent network dissectors for Wireshark to identify VPN streams under obfuscated networking ports.
* **Details:**
  * **OpenVPN Dissector (`openvpn.lua`):** Uses heuristic analysis to identify OpenVPN handshakes, parse opcodes, session IDs, ACK counts, and payload lengths.
  * **WireGuard Dissector (`wireguard.lua`):** Bonus implementation identifying WireGuard messages (Handshake Initiation/Response, Cookie Reply, Transport Data) through reserved byte validation and packet sizes.
  * Includes test PCAP files (`openvpn_capture.pcap`, `wireguard_capture.pcap`) and virtual machine server/client configuration instructions (`ReadMe`).
* **Report:** Technical results are detailed in `report.pdf` inside the folder.

---
[← Semester 2 Index](../README.md) | [← Portfolio Root](../../README.md)
