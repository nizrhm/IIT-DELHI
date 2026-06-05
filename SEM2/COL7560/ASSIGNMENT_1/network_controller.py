import subprocess
import time
import random
import threading
import numpy as np

# CONFIGURATION
INTERFACE = "eth0"  # CHANGE THIS to your active interface (e.g., wlan0, ens33)

class NetworkShaper:
    def __init__(self, interface=INTERFACE):
        self.interface = interface
        self.running = False
        self.bw_log = []
        self.lat_log = []
        self.lock = threading.Lock()

    def reset_network(self):
        """Clears existing rules."""
        cmd = f"tc qdisc del dev {self.interface} root 2> /dev/null"
        subprocess.run(cmd, shell=True)

    def apply_condition(self, bandwidth_kbps, latency_ms):
        """Applies netem rules."""
        # Using htb for rate limiting and netem for delay
        # Note: A simple netem implementation is used here for assignment simplicity
        # Command: tc qdisc replace dev eth0 root netem rate Xkbit delay Yms
        cmd = (f"tc qdisc replace dev {self.interface} root netem "
               f"rate {bandwidth_kbps}kbit delay {latency_ms}ms")
        subprocess.run(cmd, shell=True, stderr=subprocess.DEVNULL)

    def _shaper_loop(self):
        while self.running:
            # Random BW: 100kbps to 4000kbps
            bw = random.randint(100, 4000)
            # Random Latency: 20ms to 200ms
            lat = random.randint(20, 200)
            
            with self.lock:
                self.bw_log.append(bw)
                self.lat_log.append(lat)
            
            self.apply_condition(bw, lat)
            time.sleep(1)

    def start(self):
        self.reset_network()
        self.running = True
        self.bw_log = []
        self.lat_log = []
        self.thread = threading.Thread(target=self._shaper_loop)
        self.thread.start()

    def stop(self):
        self.running = False
        if hasattr(self, 'thread'):
            self.thread.join()
        self.reset_network()

    def get_stats(self):
        """Returns stats for directory naming."""
        with self.lock:
            if not self.bw_log: return 0, 0, 0, 0
            return (np.mean(self.bw_log), np.std(self.bw_log), 
                    np.mean(self.lat_log), np.std(self.lat_log))