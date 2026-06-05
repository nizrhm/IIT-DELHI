import os
import time
import datetime
import subprocess
import random
from selenium import webdriver
from selenium.webdriver.chrome.options import Options
from browsermobproxy import Server
from network_controller import NetworkShaper

# --- USER CONFIGURATION ---
BMP_PATH = "/home/nizrhu/browsermob-proxy-2.1.4/bin/browsermob-proxy"
INTERFACE = "eth0"
NUM_EXPERIMENTS = 50
# --- REPLACEMENT CODE ---
import csv

WEBSITES = []
try:
    with open('sites.csv', 'r') as f:
        # Assumes the CSV has a header row. If not, remove 'next(reader)'
        reader = csv.reader(f)
        next(reader, None)  # Skip header
        for row in reader:
            if row:
                WEBSITES.append(row[1]) # Assumes URL is in the first column
except FileNotFoundError:
    print("Error: sites.csv not found! Using fallback list.")
    WEBSITES = ["https://www.google.com", "https://www.wikipedia.org"]
# ------------------------
# --------------------------

def run_single_experiment(exp_id, proxy, shaper):
    # Setup Chrome (We restart Chrome for every exp to ensure 'clean state')
    chrome_options = Options()
    chrome_options.add_argument("--ignore-certificate-errors")
    chrome_options.add_argument(f"--proxy-server={proxy.proxy}")
    chrome_options.add_argument("--incognito")
    chrome_options.add_argument("--no-sandbox")
    chrome_options.add_argument("--disable-dev-shm-usage")
    chrome_options.add_argument("--headless=new")
    
    driver = webdriver.Chrome(options=chrome_options)
    
    # Packet Capture
    pcap_filename = f"temp_capture_{exp_id}.pcap"
    pcap_proc = subprocess.Popen(
        ["sudo", "tcpdump", "-i", INTERFACE, "-w", pcap_filename],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )

    url = random.choice(WEBSITES)
    plt = 0
    har_data = None

    try:
        print(f"[{exp_id}] Loading {url}...")
        
        # Create a fresh HAR for this experiment
        proxy.new_har(f"exp_{exp_id}", options={'captureHeaders': True, 'captureContent': True})
        
        start_time = time.time()
        driver.get(url)
        time.sleep(2) 
        end_time = time.time()
        
        plt = end_time - start_time
        har_data = proxy.har
        
        time.sleep(2) # Allow network shaper to log some data

    except Exception as e:
        print(f"Error in exp {exp_id}: {e}")
        
    finally:
        pcap_proc.terminate()
        driver.quit() # Close Chrome only

    # --- SAVE DATA ---
    avg_bw, std_bw, avg_lat, std_lat = shaper.get_stats()
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    
    folder_name = (f"experiment_{timestamp}_"
                   f"{int(avg_bw)}kbps_{int(std_bw)}kbps_"
                   f"{int(avg_lat)}ms_{int(std_lat)}ms")
    
    out_dir = os.path.join("web_browsing", folder_name)
    os.makedirs(out_dir, exist_ok=True)
    
    if os.path.exists(pcap_filename):
        os.rename(pcap_filename, os.path.join(out_dir, "capture.pcap"))
    
    if har_data:
        with open(os.path.join(out_dir, "har_log.har"), "w") as f:
            f.write(str(har_data))
            
    with open(os.path.join(out_dir, "page_load_time.csv"), "w") as f:
        f.write("url,page_load_time_sec\n")
        f.write(f"{url},{plt}\n")
        
    print(f"Saved: {out_dir}")


if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Please run with sudo: sudo ./venv/bin/python3 part1_web.py")
        exit(1)

    print("--- Initializing Server and Shaper (ONCE) ---")
    
    # 1. Start Proxy Server ONCE
    server = Server(BMP_PATH, options={'port': 9090})
    server.start()
    proxy = server.create_proxy()
    
    # 2. Start Network Shaper ONCE
    shaper = NetworkShaper(INTERFACE)
    shaper.start()
    
    try:
        # Run the loop
        for i in range(NUM_EXPERIMENTS):
            print(f"Starting Experiment {i+1}/{NUM_EXPERIMENTS}")
            run_single_experiment(i + 1, proxy, shaper)
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        print("--- Cleaning up Server and Shaper ---")
        shaper.stop()
        server.stop()