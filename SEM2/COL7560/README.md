# COL7560: Advanced Computer Networks (ML in Networks Focus)

This directory contains assignments and project work completed for **COL7560: Advanced Computer Networks** at IIT Delhi. This course focuses on utilizing machine learning techniques to monitor, analyze, and optimize computer networks.

---

## 📁 Course Structure

### 📈 [ASSIGNMENT 1: Active Data Collection & QoE Analysis](./ASSIGNMENT_1/)
* **Objective:** Collect packet traces and application-level Quality of Experience (QoE) metrics under controlled network conditions.
* **Methodology:**
  * Uses browser-automation tools (Selenium) to load popular websites, prompt ChatGPT, and stream video content.
  * Emulates varying network conditions (bandwidth, latency, and packet loss) dynamically at 1-second intervals using Linux Traffic Control (`tc` & `shaper.sh`).
  * Extracts flow features (packet size, Inter-Arrival Time / IAT, throughput, and duration) using `tshark`/`scapy`.
  * Performs correlation analysis between network conditions and QoE metrics.

### 🤖 [ASSIGNMENT 2: Video QoE ML Prediction](./ASSIGNMENT_2/)
* **Objective:** Build regression models to predict video streaming QoE metrics directly from packet-level CSV files.
* **Key Tasks:**
  * Implements `feature_extractor.py` containing custom feature engineering for four different target metrics: average resolution, rebuffering ratio, startup latency, and bitrate switches per second.
  * Utilizes `scikit-learn` and saved pipeline estimators (`models.pkl` and `scalers.pkl`) to make predictions on packet streams under varying TCP and QUIC configurations.

### ⚡ [PROJECT: EarlyFlow Traffic Classification](./PROJECT/earlyflow/)
* **Objective:** Early Time Series Classification (ETSC) of mobile network traffic flows using per-packet feature streams.
* **Description:**
  * Classifies internet flows after observing only a minimal number of packets (e.g., first 5–10 packets) to allow early firewalls, QoS routing, and traffic shaping actions.
  * Implements the **CALIMERA** mechanism alongside a **MiniRocket** time series classifier model.
  * Classifies traffic into four application categories: Web, Streaming, Social Media, and Cloud.

---
[← Semester 2 Index](../README.md) | [← Portfolio Root](../../README.md)
