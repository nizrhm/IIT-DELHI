import time
import json
import random
import os
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC

# List of random prompts to use
PROMPTS = [
    "Explain quantum entanglement in simple terms",
    "Write a haiku about a traffic jam",
    "What are the health benefits of green tea?",
    "Generate a random Python dictionary",
    "Explain the difference between TCP and UDP",
    "Write a short story about a robot who loves gardening",
    "What is the capital of Australia?",
    "Summarize the plot of Romeo and Juliet",
    "How do I cook a perfect steak?",
    "Explain the concept of inflation in economics"
]

def get_driver(profile_path):
    options = Options()
    options.add_argument(f"user-data-dir={profile_path}")
    options.add_argument("--disable-blink-features=AutomationControlled")
    options.add_experimental_option("excludeSwitches", ["enable-automation"])
    options.add_experimental_option('useAutomationExtension', False)
    
    # Enable performance logging for HAR-like data
    options.set_capability('goog:loggingPrefs', {'performance': 'ALL'})
    
    driver = webdriver.Chrome(options=options)
    return driver

def run_chatgpt_experiment(experiment_id, output_dir):
    # Absolute path to your profile folder
    profile_path = os.path.abspath("chrome_profile")
    
    driver = get_driver(profile_path)
    response_time = 0
    
    try:
        driver.get("https://chatgpt.com")
        
        # 1. Find the text area
        # Note: The ID 'prompt-textarea' is common, but might change. 
        # Using a reliable CSS selector for the editable div.
        wait = WebDriverWait(driver, 10)
        text_area = wait.until(EC.presence_of_element_located((By.ID, "prompt-textarea")))
        
        # 2. Pick a prompt and send it
        prompt = random.choice(PROMPTS)
        print(f"[*] Sending prompt: {prompt}")
        
        text_area.clear()
        text_area.send_keys(prompt)
        time.sleep(0.5)
        
        # Start Timer just before sending
        start_time = time.time()
        text_area.send_keys(Keys.ENTER)
        
        # 3. Wait for response to complete
        # We detect this by waiting for the "Stop generating" button to disappear 
        # OR the "Send" button to reappear/become enabled.
        # A simple heuristic is to wait for the "Copy" icon to appear in the last message.
        
        # Wait 2 seconds for generation to actually start
        time.sleep(2) 
        
        # Wait until the 'stop generating' button is GONE (meaning it finished)
        # The selector for the stop button usually contains 'stop' or has specific SVG
        # A safer generic check is checking for the "send" button to be ready again
        wait.until(EC.presence_of_element_located((By.CSS_SELECTOR, "button[data-testid='send-button']")))
        
        end_time = time.time()
        response_time = (end_time - start_time) * 1000 # to ms
        
        print(f"[*] Response time: {response_time:.2f} ms")

        # 4. Save Logs
        # Save Response Time
        with open(f"{output_dir}/response_time.csv", "w") as f:
            f.write("prompt,response_time_ms\n")
            f.write(f'"{prompt}",{response_time}\n')

        # Save HAR (Performance Logs)
        logs = driver.get_log('performance')
        with open(f"{output_dir}/har_log.har", "w") as f:
            f.write(json.dumps(logs))

    except Exception as e:
        print(f"[!] Error in Selenium: {e}")
        # Save error to csv so we know it failed
        with open(f"{output_dir}/response_time.csv", "w") as f:
            f.write("error\n")
            f.write(str(e))
            
    finally:
        driver.quit()