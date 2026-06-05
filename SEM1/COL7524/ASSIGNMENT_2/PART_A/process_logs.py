# process_logs.py
import pandas as pd
import matplotlib.pyplot as plt
import glob
import os

# ---------------- SETTINGS ----------------
log_dir = "logs"
schedulers = ["fcfs", "sjf", "rr"]
window_ms = 1000  # throughput window in ms

# ---------------- LOAD DATA ----------------
data = {}
for sched in schedulers:
    path = os.path.join(log_dir, f"{sched}_log.csv")
    if os.path.exists(path):
        df = pd.read_csv(path, names=["RequestID","Command","Arrival_us","Start_us","Finish_us","Waiting_ms","Response_ms"])
        df = df.sort_values("Arrival_us")
        df['Arrival_ms'] = df['Arrival_us'] / 1000.0
        df['Start_ms'] = df['Start_us'] / 1000.0
        df['Finish_ms'] = df['Finish_us'] / 1000.0
        data[sched] = df
    else:
        print(f"[WARN] Log file not found for {sched}: {path}")

# ---------------- PLOT Waiting & Response ----------------
plt.figure(figsize=(12,6))
for sched, df in data.items():
    plt.plot(df['RequestID'], df['Waiting_ms'], label=f'{sched.upper()} Waiting Time')
    plt.plot(df['RequestID'], df['Response_ms'], linestyle='--', label=f'{sched.upper()} Response Time')
plt.xlabel('Request ID')
plt.ylabel('Time (ms)')
plt.title('Server Waiting & Response Times Comparison')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("waiting_response_comparison.png")
plt.show()

# ---------------- PLOT Throughput ----------------
plt.figure(figsize=(12,6))
for sched, df in data.items():
    times = df['Finish_ms'].values
    throughput = []
    for t in times:
        count = ((times >= t - window_ms) & (times <= t)).sum()
        throughput.append(count * (1000 / window_ms))  # req/sec
    plt.plot(df['RequestID'], throughput, label=f'{sched.upper()} Throughput')
plt.xlabel('Request ID')
plt.ylabel('Throughput (req/sec)')
plt.title('Server Throughput Comparison')
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig("throughput_comparison.png")
plt.show()

# ---------------- PRINT SUMMARY ----------------
for sched, df in data.items():
    print(f"\n--- {sched.upper()} ---")
    print(f"Total Requests: {len(df)}")
    print(f"Average Waiting Time: {df['Waiting_ms'].mean():.3f} ms")
    print(f"Median Waiting Time: {df['Waiting_ms'].median():.3f} ms")
    print(f"Average Response Time: {df['Response_ms'].mean():.3f} ms")
    print(f"Median Response Time: {df['Response_ms'].median():.3f} ms")
