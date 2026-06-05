import os
import time
import datetime
import subprocess
import random
import csv
import socket
import numpy as np
from selenium import webdriver
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from browsermobproxy import Server
from network_controller import NetworkShaper
from pyvirtualdisplay import Display

# --- CONFIGURATION ---
BMP_PATH = "/home/nizrhu/browsermob-proxy-2.1.4/bin/browsermob-proxy"
INTERFACE = "eth0"
NUM_EXPERIMENTS = 50
# The URL confirmed to work by your debug script
DASH_PLAYER_URL = "https://reference.dashif.org/dash.js/nightly/samples/dash-if-reference-player/index.html"

# --- CONFIGURATION ---
# --- CONFIGURATION ---
VIDEO_SOURCES = [
    # 1. Envivio (Animation - Skiing/Sports)
    # Status: High Reliability (Akamai)
    "https://dash.akamaized.net/envivio/EnvivioDash3/manifest.mpd",

    # 2. LiveSim (Test Pattern with Ticking Clock)
    # Status: High Reliability (DASH-IF)
    "https://livesim.dashif.org/livesim/testpic_2s/Manifest.mpd",

    # 3. Tears of Steel (Sci-Fi Movie - Real humans/CGI)
    # Status: High Reliability (Unified Streaming)
    "https://demo.unified-streaming.com/k8s/features/stable/video/tears-of-steel/tears-of-steel.ism/.mpd",

    # 4. Big Buck Bunny (Akamai - High Reliability)
    "https://dash.akamaized.net/akamai/bbb_30fps/bbb_30fps.mpd",

    # 5. Elephant's Dream (Surreal 3D Animation)
    # Status: Medium Reliability (Akamai)
    "https://dash.akamaized.net/akamai/test/caption_test/ElephantsDream/elephants_dream_480p_heaac5_1.mpd"
]

# --- HELPER: Find a free port automatically ---
def get_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(('', 0))
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        return s.getsockname()[1]
# ---------------------------------------------

def run_single_experiment(exp_id, proxy, shaper):
    chrome_options = Options()
    chrome_options.add_argument("--ignore-certificate-errors")
    chrome_options.add_argument(f"--proxy-server={proxy.proxy}")
    chrome_options.add_argument("--autoplay-policy=no-user-gesture-required")
    chrome_options.add_argument("--no-sandbox")
    chrome_options.add_argument("--disable-dev-shm-usage")
    # Virtual Display handles the screen, so we start maximized inside it
    chrome_options.add_argument("--start-maximized")
    
    driver = webdriver.Chrome(options=chrome_options)
    
    pcap_filename = f"temp_video_{exp_id}.pcap"
    pcap_proc = subprocess.Popen(
        ["sudo", "tcpdump", "-i", INTERFACE, "-w", pcap_filename],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
    )
    
    metrics = [] 
    har_data = None
    
    try:
        proxy.new_har(f"video_exp_{exp_id}")
        driver.get(DASH_PLAYER_URL)
        
        wait = WebDriverWait(driver, 20)
        
        # --- FIX: USE THE SELECTOR CONFIRMED BY DEBUGGER ---
        url_input = wait.until(EC.element_to_be_clickable((By.CSS_SELECTOR, "input[type='text']")))
        
        video_url = random.choice(VIDEO_SOURCES)
        print(f"[{exp_id}] Streaming: {video_url}")
        
        url_input.clear()
        url_input.send_keys(video_url)
        
        # Click Load (Robust find by text)
        load_btn = driver.find_element(By.XPATH, "//div[contains(text(), 'Load')] | //button[contains(text(), 'Load')]")
        load_btn.click()
        
        # Wait for video tag to appear
        wait.until(EC.presence_of_element_located((By.TAG_NAME, "video")))
        time.sleep(5) # Buffer time
        
        # STREAM FOR 2 MINUTES
        start_time = time.time()
        while (time.time() - start_time) < 120:
            try:
                # --- REPLACEMENT JAVASCRIPT ---
                script = """
                    try {
                        var height = 0;
                        var buffer = 0;
                        
                        // 1. Get Resolution directly from the HTML5 Video element
                        // This works regardless of the DashJS version or internal API changes
                        var vid = document.querySelector('video');
                        if (vid) {
                            height = vid.videoHeight;
                        }
                        
                        // 2. Get Buffer from the global 'player' object
                        // The DashIF reference player exposes 'player' globally
                        if (window.player && typeof window.player.getBufferLength === 'function') {
                            buffer = window.player.getBufferLength('video');
                        }
                        
                        return [height, buffer];
                    } catch(e) { return [0, 0]; }
                """
                data = driver.execute_script(script)
                if data:
                    metrics.append([time.time(), data[0], data[1]])
            except:
                pass
            time.sleep(1) 
            
        har_data = proxy.har

    except Exception as e:
        print(f"Error in exp {exp_id}: {e}")

    finally:
        pcap_proc.terminate()
        driver.quit()

    # --- SAVE DATA ---
    avg_bw, std_bw, avg_lat, std_lat = shaper.get_stats()
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    
    folder_name = (f"experiment_{timestamp}_"
                   f"{int(avg_bw)}kbps_{int(std_bw)}kbps_"
                   f"{int(avg_lat)}ms_{int(std_lat)}ms")
    
    out_dir = os.path.join("video_streaming", folder_name)
    os.makedirs(out_dir, exist_ok=True)
    
    if os.path.exists(pcap_filename):
        os.rename(pcap_filename, os.path.join(out_dir, "capture.pcap"))
        
    if har_data:
        with open(os.path.join(out_dir, "har_log.har"), "w") as f:
            f.write(str(har_data))

    if metrics:
        # Sanitize data: Treat None as 0 to prevent "NoneType < float" errors
        resolutions = [(m[1] if m[1] is not None else 0) for m in metrics]
        buffers = [(m[2] if m[2] is not None else 0) for m in metrics]
        
        avg_resolution = np.mean(resolutions) if resolutions else 0
        
        # Now we can safely compare b < 0.1 because we ensured b is a number
        rebuffer_count = sum(1 for b in buffers if b < 0.1)
        rebuffer_ratio = rebuffer_count / len(buffers)
    else:
        avg_resolution = 0
        rebuffer_ratio = 0
        
    with open(os.path.join(out_dir, "player_metrics.csv"), "w", newline='') as f:
        writer = csv.writer(f)
        writer.writerow(["timestamp", "resolution", "buffer_occupancy", "calc_avg_res", "calc_rebuffer_ratio"])
        for m in metrics:
            writer.writerow([m[0], m[1], m[2], avg_resolution, rebuffer_ratio])

    print(f"Saved: {out_dir}")


if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Please run with sudo: sudo ./venv/bin/python3 part3_video.py")
        exit(1)

    print("--- Initializing Server ---")
    
    # REMOVED: display = Display(...)
    # REMOVED: display.start()
    
    # 2. Dynamic Port Assignment
    free_port = get_free_port()
    print(f"--- Starting Proxy on Port {free_port} ---")
    
    server = Server(BMP_PATH, options={'port': free_port})
    server.start()
    proxy = server.create_proxy()
    
    # 3. Start Shaper
    shaper = NetworkShaper(INTERFACE)
    shaper.start()

    try:
        for i in range(NUM_EXPERIMENTS):
            print(f"Starting Video Experiment {i+1}/{NUM_EXPERIMENTS}")
            run_single_experiment(i + 1, proxy, shaper)
    except KeyboardInterrupt:
        print("\nStopping...")
    finally:
        print("--- Cleaning up ---")
        shaper.stop()
        server.stop()
        # REMOVED: display.stop()