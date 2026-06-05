# setup_login.py
from selenium import webdriver
from selenium.webdriver.chrome.options import Options
import os

# Get absolute path to the profile directory
profile_path = os.path.abspath("chrome_profile")

options = Options()
options.add_argument(f"user-data-dir={profile_path}")

# These flags help bypass detection
options.add_argument("--disable-blink-features=AutomationControlled") 
options.add_experimental_option("excludeSwitches", ["enable-automation"])
options.add_experimental_option('useAutomationExtension', False)

driver = webdriver.Chrome(options=options)
driver.get("https://chatgpt.com")

print("Please log in manually. Once logged in, close the browser to save the session.")
# Keep script running until you close the window
input("Press Enter here after you have closed the browser window...")