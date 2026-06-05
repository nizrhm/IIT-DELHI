# IIT Delhi — M.Tech Computer Science & Engineering Portfolio

Welcome to my academic projects repository. This repository serves as a comprehensive showcase of projects, assignments, and lab work completed during my M.Tech in Computer Science and Engineering at the **Indian Institute of Technology, Delhi (IIT Delhi)**.

---

## 📂 Repository Structure

The projects are organized by semester. Click on a semester or a specific course below to explore the codebase, documentation, and reports.

| Semester | Course Code | Course Title | Key Projects / Content |
| :--- | :--- | :--- | :--- |
| **[Semester 1](./SEM1/)** | [COL7524](./SEM1/COL7524/) | Advanced Computer Networks | BGP Routing Simulations (NS-3) |
| **[Semester 2](./SEM2/)** | [COD7001](./SEM2/COD7001/) | Software Systems Laboratory | `minigdb` Debugger, Mini UNIX Shell, Project Report & Demo |
| | [COL7560](./SEM2/COL7560/) | Advanced Computer Networks (ML Focus) | **EarlyFlow** (Early Network Classification) & Active Measurement |
| | [COL8395](./SEM2/COL8395/) | Special Topics in Vector Search & DBMS | **Adaptive Vector DBMS** (PQ, OPQ, AQ & FAISS Reranking) |
| | [COL8585](./SEM2/COL8585/) | System and Network Security | **IronDome** (Port-Independent OpenVPN/WireGuard Dissectors) |
| | [SIL7165](./SEM2/SIL7165/) | Network and System Security | **S.H.I.E.L.D.** (Side-Channel Defense for LLMs) & Crypto Solvers |
| | [VEV7031](./SEM2/VEV7031/) | Value Education / Professional Ethics | SATTVIC Reports and Ethics Essays |
| | [VentureStudio](./SEM2/VentureStudio/) | Entrepreneurship & Venture Studio | **AutoCEM** Business Plan & Pitch deck |

---

## 🌟 Highlighted Projects

Here are some of the most notable projects contained within this repository:

### 🛡️ [S.H.I.E.L.D. — LLM Side-Channel Defense](./SEM2/SIL7165/Project/)
* **Category:** Network & System Security / AI Security
* **Concept:** A plug-and-play defense layer designed to neutralize timing side-channel vulnerabilities, multi-modal jailbreaks, and autonomous red-teaming attacks targeting LLMs.
* **Core Mechanisms:** Constant-Time Token Emission (CTE), Stochastic Jitter Injection (SJI), Active Stream Masking (ASM), and Constant-Size Network Packet Padding.

### ⚡ [EarlyFlow — Early Network Traffic Classification](./SEM2/COL7560/PROJECT/earlyflow/)
* **Category:** Advanced Networking / Machine Learning
* **Concept:** Classifies network flows as early as possible (using only the first few packets) to facilitate real-time QoS routing, firewall policies, and anomaly detection.
* **Core Mechanisms:** Leverages **CALIMERA** and **MiniRocket** models to classify flows into cloud, social media, streaming, or web categories with minimal packet traces.

### 🔍 [Adaptive Vector DBMS using FAISS](./SEM2/COL8395/PROJECT/)
* **Category:** Databases / Machine Learning Systems
* **Concept:** An approximate nearest neighbor (ANN) search engine implementing Product Quantization (PQ), Optimized PQ (OPQ), and Additive Quantization (AQ) under Inverted File (IVF) index.
* **Key Contribution:** Implements an **Adaptive Reranking** mechanism that dynamically decides candidate pool size based on search difficulty, optimizing accuracy vs. latency trade-off.

### 🛡️ [IronDome — VPN Traffic Analyzer](./SEM2/COL8585/ASSIGNMENT_2/)
* **Category:** System & Network Security
* **Concept:** Port-independent detection and packet dissection of OpenVPN and WireGuard traffic using custom LUA plugins inside Wireshark.
* **Key Features:** Supports both offline PCAP forensics and live network interface packet interception.

### 🐚 [minigdb & Mini UNIX Shell](./SEM2/COD7001/)
* **Category:** Systems Programming / OS
* **Concept:** Low-level implementation of a Linux ELF debugger (`minigdb`) using the `ptrace` system call, and a modular command shell supporting multiple pipeline stages, background tasks, and redirection.

---

## 🛠️ Technology Stack & Skills Illustrated

* **Systems Programming:** C, Shell scripting (Bash/Powershell), Makefile, Linux Kernel (`ptrace`), Multiprocessing & Pipes.
* **Networking & Security:** Packet Dissection (Wireshark, Lua), Network Simulation (NS-3), Network Traffic Shaping (`tc`), PCAP Analysis.
* **AI & Security:** LLM API Defenses, Stream Masking, Timing Obfuscation, Adversarial Jailbreaks.
* **Data Science & ML:** Python, NumPy, FAISS, MiniRocket, Scikit-learn, Vector Embeddings.
* **Professional:** Venture creation, Ethics, Technical reporting, Data-driven analysis.

---

*For detailed setup instructions, code execution guides, and technical reports, please navigate to the corresponding course directories.*
