import time, csv, random
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.common.keys import Keys
from selenium.webdriver.chrome.options import Options
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC

PROMPTS = [
    "Explain TCP congestion control",
    "What is active measurement?",
    "Difference between latency and throughput",
    "Explain HTTPS",
    "How does adaptive bitrate streaming work?"
]

opts = Options()
opts.add_argument("--incognito")
opts.set_capability("goog:loggingPrefs", {"performance": "ALL"})

driver = webdriver.Chrome(options=opts)
wait = WebDriverWait(driver, 120)

driver.get("https://chat.openai.com/")

textbox = wait.until(
    EC.presence_of_element_located((By.TAG_NAME, "textarea"))
)

prompt = random.choice(PROMPTS)

start = time.time()
textbox.send_keys(prompt)
textbox.send_keys(Keys.ENTER)

wait.until(
    EC.presence_of_element_located((By.XPATH, "//div[contains(@class,'markdown')]"))
)
time.sleep(5)
end = time.time()

with open("response_time.csv", "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["response_time_sec"])
    w.writerow([end - start])

logs = driver.get_log("performance")
with open("har_log.har", "w") as f:
    for l in logs:
        f.write(str(l) + "\n")

driver.quit()
