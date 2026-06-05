# SIL7165: Network and System Security

This directory contains lab assignments and the capstone project for **SIL7165: Network and System Security** at IIT Delhi. The coursework covers cryptographic solvers, network firewalls, system configuration benchmarks, and modern AI endpoint defenses.

---

## 📁 Course Structure

### 🔓 [ASSIGNMENT 1: Cryptographic Solver](./ASSIGNMENT_1/)
* **Objective:** Crack classic substitution and polyalphabetic ciphers.
* **Details:**
  * Implements `solver.py`, a frequency-based decryption assistant.
  * Uses **English quadgram statistics** (`english_quadgrams.txt`) to evaluate potential key decryptions through log-likelihood scoring.
  * Solves multi-stage cipher files (`cipher-4.txt`, `cipher-5.txt`, `cipher-6.txt`) to restore original English plaintexts automatically.

### 🛡️ [ASSIGNMENT 2: Host Defense & Networking Labs](./ASSIGNMENT_2/)
* **Objective:** Secure Linux hosts against unauthorized access and traffic profiling.
* **Details:**
  * Includes configurations for user directories, host authorization rules (`id_rsa` keys), and networking environments.
  * Organizes labs `P1` through `P5` according to assignments specified in `NSS_Assignment_2.pdf`.

### 📓 [ASSIGNMENT 3: Network Security Analysis](./ASSIGNMENT_3/)
* **Objective:** Interactive data-driven network traffic security audits.
* **Details:**
  * Contains Jupyter Notebook files (`NSS_ASSIGN3_1.ipynb` to `NSS_ASSIGN3_4.ipynb`) covering traffic statistics, network classification, and anomaly detection models.

### 🤖 [PROJECT: S.H.I.E.L.D. — LLM Side-Channel Defense](./Project/)
* **Objective:** Protect Large Language Model streaming APIs from side-channel attacks.
* **Key Defensive Mechanisms:**
  * **CTE (Constant-Time Emission):** Standardizes token emission intervals to neutralize statistical timing inferences.
  * **SJI (Stochastic Jitter Injection):** Introduces controlled Laplace noise to timing signatures.
  * **ASM (Active Stream Masking):** Mitigates "MasterKey" moderation bypass attacks by hot-swapping stalls with mock streaming refusals.
  * **Padding:** Pads encrypted transport packets to fixed-size boundaries.
* **Go to Project README:** For system details, Mermaid workflows, CLI flags, and testing instructions, see [Project/README.md](./Project/README.md).

---
[← Semester 2 Index](../README.md) | [← Portfolio Root](../../README.md)
