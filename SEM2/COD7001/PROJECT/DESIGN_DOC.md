# Resource Manager - Technical Design Document

## 1. System Architecture Overview

The **User-Level Resource-Aware Runtime System** simulates a lightweight kernel execution environment entirely in user space. It manages tasks, schedules resources (CPU time and Memory), detects deadlocks, enforces timeouts, and dynamically adapts scheduling rules based on real-time system metrics captured from the host machine.

The architecture revolves around a centralized **Runtime System** that orchestrates various sub-components:
- **Scheduler**: Dictates task execution order and handles state transitions.
- **Monitor**: Gathers system utilization metrics using an adaptive sampling frequency.
- **Policy Engine**: High-level logic that selects the optimal scheduling philosophy.
- **Metrics Trackers**: Logs and evaluates scheduler performance for post-run analysis.
- **Watchdog & Deadlock Detector**: Ensures system health and safety-state compliance.

---

## 2. Core Components

### 2.1 The `Runtime` Environment
The `Runtime` class is the central nervous system of the project. it manages the global simulation clock and coordinates the execution pulse.
- **PID-based Throttling:** To maintain system stability, the runtime implements a **PID (Proportional-Integral-Derivative) Controller**. Using tuned constants ($K_p=0.8, K_i=0.1, K_d=0.05$), it calculates an error signal based on host CPU pressure and applies proportional micro-delays (up to 100ms) to stabilize the simulation.
- **Multicore Support:** Supports up to 4 configured cores (`MAX_CORES`), distributing the ready-queue tasks using efficient task-to-core mapping.

### 2.2 Task Representation (`Task` Class)
The `Task` class encapsulates the state and lifecycle of a process.
- **Burst Sequences:** Tasks are modeled as a sequence of CPU and I/O bursts (e.g., `C10;I5;C20`).
- **Resource Profiling:** Each task tracks its `simulated_memory_used`. The system proactively flags residual memory at termination as a potential leak.
- **Fault Injection:** Supports a "Buggy" state used to validate the Watchdog's ability to kill stalled or infinite-loop processes.

### 2.3 The `Scheduler`
The `Scheduler` manages five distinct policies, allowing the system to handle varied workloads:
- **FIFO**: Simple arrival-ordered execution.
- **ROUNDROBIN**: Preemption with a fixed quantum ($Q=2$).
- **PRIORITY**: Strict priority adherence with an **Aging Mechanism** (Priority boost triggered after **50 ticks** of waiting).
- **MLFQ (Multi-Level Feedback Queue)**: Features a 3-level queue structure with $2^n$ scaling time quanta:
    - **L0 (High Prio)**: $Q=2$
    - **L1 (Med Prio)**: $Q=4$
    - **L2 (Low Prio)**: $Q=8$
- **ADAPTIVE**: Dynamically switches the active policy (e.g., transitioning to RR during high concurrency) based on resource thresholds.

### 2.4 Safety and Health Enforcement
- **Banker's Algorithm**: The `DeadlockDetector` performs a safety state simulation before every resource grant. It uses **Available**, **Max Demand**, and **Allocation** matrices to guarantee no circular wait conditions occur.
- **Integrated Deadlock Recovery**: If the system detects a "wedged" state (no progress for **500 ticks**), it automatically terminates the lowest-priority task to break the dependency cycle.
- **Watchdog**: Periodically verifies `last_run_tick` across all cores. Detects unresponsive tasks and provides a fail-safe termination mechanism.

### 2.5 Monitor and IPC Modeling
- **Adaptive Sampling Frequency**: The `Monitor` reduces the "Observer Effect" by scaling its sampling interval from **5 to 50 cycles** depending on system volatility.
- **Predictive Resource Guard**: Performs slope-based trend analysis. Rapid CPU surges (slope $> 15.0$) trigger proactive throttling to prevent system destabilization.
- **Inter-Task Communication (IPC)**: Simulates a **Mailbox System** where tasks send "Heartbeat Sync" messages every **100 cycles**, demonstrating concurrent communication and synchronization logic.

---

## 3. Data Flow & Execution Model

1.  **Admission:** Tasks enter the system from `workload.csv` and wait in the `pendingTasks` bin until their arrival time.
2.  **Monitoring:** At every sample point, the Monitor captures CPU/Memory metrics. The Predictive Guard evaluates trends for early-warning triggers.
3.  **Scheduling Decision:** The Policy Engine decides the active policy. The Scheduler then performs the safety check via the Deadlock Detector.
4.  **Pulse Execution:** Tasks execute on available core slots. If a task depletes its time quantum (in RR or MLFQ), it is preempted.
5.  **Telemetry:** Performance metrics (throughput, context switches, wait time) are updated and eventually saved to `evaluation_results.csv`.

---

## 4. Evaluation and Benchmarking
The evaluation suite (`-test`) provides a rigorous comparison across all five policies using an identical workload. This highlights the trade-off between the low-overhead stability of FIFO and the fairness/interactivity of MLFQ and Round Robin.
