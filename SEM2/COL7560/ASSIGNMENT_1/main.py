import os
import time
import subprocess
import threading
import random
import numpy as np
from datetime import datetime
from chatgpt_task import run_chatgpt_experiment

# --- Network Controller (Manages shaper.sh) ---
class NetworkController:
    def __init__(self, interface, shaper_path="./shaper.sh"):
        self.interface = interface
        self.shaper_path = shaper_path
        self.stop_event = threading.Event()
        self.stats = [] 
        self.thread = None

    def _generate_conditions(self):
        # BW: 100kbps (0.1) to 4Mbps (4.0)
        bw = round(random.uniform(0.1, 4.0), 2)
        # Latency: 20ms to 200ms
        delay = int(random.uniform(20, 200))
        return bw, delay

    def _run_shaping_loop(self):
        while not self.stop_event.is_set():
            bw, delay = self._generate_conditions()
            self.stats.append((bw, delay))
            # ./shaper.sh update <iface> <down> <up> <delay>
            cmd = ["sudo", self.shaper_path, "update", self.interface, str(bw), str(bw), str(delay)]
            subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(1)

    def start(self):
        bw, delay = self._generate_conditions()
        # Initial start
        cmd = ["sudo", self.shaper_path, "start", self.interface, str(bw), str(bw), str(delay)]
        subprocess.run(cmd, check=True)
        
        self.stop_event.clear()
        self.stats = [] # Reset stats
        self.thread = threading.Thread(target=self._run_shaping_loop)
        self.thread.start()

    def stop(self):
        self.stop_event.set()
        if self.thread: self.thread.join()
        subprocess.run(["sudo", self.shaper_path, "stop", self.interface], check=True)

    def get_stats(self):
        if not self.stats: return (0, 0, 0, 0)
        bws = [x[0] for x in self.stats]
        delays = [x[1] for x in self.stats]
        return (np.mean(bws), np.std(bws), np.mean(delays), np.std(delays))

# --- Main Experiment Loop ---
def main():
    INTERFACE = "eth0"  # <--- CHANGE THIS to your interface (e.g., wlan0)
    TOTAL_EXPERIMENTS = 50
    
    print(f"[*] Starting {TOTAL_EXPERIMENTS} experiments on {INTERFACE}...")

    for i in range(1, TOTAL_EXPERIMENTS + 1):
        print(f"\n--- Experiment {i}/{TOTAL_EXPERIMENTS} ---")
        
        # 1. Setup Temp Directory
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        temp_dir = f"temp_{timestamp}"
        os.makedirs(temp_dir, exist_ok=True)
        
        # 2. Start TCPDUMP
        pcap_file = f"{temp_dir}/capture.pcap"
        # Filter out SSH (port 22) to avoid capturing your own control traffic
        cmd_dump = ["sudo", "tcpdump", "-i", INTERFACE, "-w", pcap_file, "not", "port", "22"]
        proc_dump = subprocess.Popen(cmd_dump, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        # 3. Start Traffic Shaping
        shaper = NetworkController(INTERFACE)
        shaper.start()
        
        # 4. Run Selenium Task
        # We wait a sec for shaping to kick in
        time.sleep(2) 
        run_chatgpt_experiment(i, temp_dir)
        
        # 5. Cleanup
        shaper.stop()
        subprocess.run(["sudo", "kill", str(proc_dump.pid)]) # Kill tcpdump
        time.sleep(1) # Allow file closure
        
        # 6. Rename Folder based on Stats
        avg_bw, std_bw, avg_lat, std_lat = shaper.get_stats()
        
        # Format: experiment_YYYYMMDD_HHMMSS_avgBW_stdBW_avgLat_stdLat
        folder_name = (f"experiment_{timestamp}_"
                       f"{avg_bw*1000:.0f}kbps_{std_bw*1000:.0f}kbps_"
                       f"{avg_lat:.0f}ms_{std_lat:.0f}ms")
        
        final_path = f"data/chatgpt/{folder_name}"
        os.makedirs(os.path.dirname(final_path), exist_ok=True)
        os.rename(temp_dir, final_path)
        
        print(f"[✓] Saved to {final_path}")
        
        # Small cooldown between experiments to avoid rate limits
        time.sleep(5)

if __name__ == "__main__":
    main()