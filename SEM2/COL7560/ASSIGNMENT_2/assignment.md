# Video QoE Prediction Assignment

## Overview

In this assignment, you will build a machine learning model that predicts the Quality of Experience (QoE) of video streaming sessions using network-level measurements. The goal is to understand how characteristics of network traffic translate into the user's streaming experience.

## Dataset

You will use a dataset derived from YouTube video streaming sessions collected using a real Android smartphone under controlled network conditions. For each session, the dataset includes packet-level traces of the video traffic. The sessions cover a wide range of bandwidth conditions—from constrained mobile-like links to high-capacity networks—and include both TCP and QUIC based streaming.

**Important**: You will only receive the **training data**. The test data is hidden and your code will be evaluated on it automatically by Gradescope.

### QoE Metrics (Prediction Targets)

| Metric | Description | Typical Range |
|--------|-------------|---------------|
| `avg_resolution` | Time-weighted average video resolution in pixels | 144-2160 |
| `rebuffering_ratio` | Fraction of session time spent rebuffering | 0-1 |
| `startup_latency` | Seconds from session start to first frame playback | 0-60+ |
| `bitrate_switches_per_second` | Number of quality changes per second of playback | 0-1 |

### Network Traffic Format

Each session has a `video_traffic.csv` file containing packet-level data:

| Column | Description |
|--------|-------------|
| `timestamp` | Unix timestamp of the packet (float) |
| `ipSrc`, `ipDst` | Source and destination IP addresses |
| `tcpPortSrc`, `tcpPortDst` | TCP ports (empty if UDP) |
| `udpPortSrc`, `udpPortDst` | UDP ports (empty if TCP) |
| `tcpLen`, `udpLen` | Payload length in bytes |
| `payloadProtocolNumber` | 6 for TCP, 17 for UDP |

## Your Task

1. **Feature Engineering**: Design and implement feature extraction functions that compute meaningful features from network traffic data. You should implement separate feature extractors for each QoE metric:
   - `extract_features_resolution()` - for predicting average resolution
   - `extract_features_rebuffering()` - for predicting rebuffering ratio
   - `extract_features_startup()` - for predicting startup latency
   - `extract_features_switches()` - for predicting bitrate switches

2. **Model Training**: Train regression models to predict each of the four QoE metrics from your extracted features.

3. **Submission**: Submit your code to Gradescope. The autograder will run your feature extractors on hidden test data and evaluate your models' predictions.

## Submission

### Files to Submit

Upload the following files to Gradescope:

| File | Description | Required |
|------|-------------|----------|
| `feature_extractor.py` | Your feature extraction code with all four metric-specific extractors | **Yes** |
| `models.pkl` | Dictionary of trained models: `{metric_name: model}` | **Yes** |
| `scalers.pkl` | Dictionary of feature scalers: `{metric_name: scaler}` | Optional |

### What the Autograder Does

1. Imports your `feature_extractor.py`
2. For each QoE metric, calls the corresponding extractor function (e.g., `extract_features_resolution()`)
3. Loads your `models.pkl` and `scalers.pkl` (if provided)
4. Generates predictions on hidden test sessions
5. Computes RMSE and scores for each metric

### Submission Checklist

Before submitting, verify that:
- [ ] `feature_extractor.py` contains all four functions: `extract_features_resolution()`, `extract_features_rebuffering()`, `extract_features_startup()`, `extract_features_switches()`
- [ ] Each function takes a file path as input and returns a dictionary of features
- [ ] `models.pkl` is a dictionary with keys: `'avg_resolution'`, `'rebuffering_ratio'`, `'startup_latency'`, `'bitrate_switches_per_second'`
- [ ] Your code runs without errors on the provided training data
- [ ] You have tested locally using `evaluate_local.py`

## Evaluation

### Grading Breakdown

| Component | Weight | Description |
|-----------|--------|-------------|
| **Demo** | 60% | Present your approach, explain your feature engineering choices, and demonstrate your code |
| **Leaderboard Performance** | 40% | Relative performance compared to other submissions on the hidden test set |

### Leaderboard Scoring

For the leaderboard component, your submission is scored based on prediction accuracy.

**Note**: Each submission is evaluated on a **random subset** of test sessions. This means your score may vary slightly between submissions even with the same code.

### Demo Expectations

During the demo, you should be prepared to:
- Explain your feature engineering approach and why you chose specific features
- Describe your model selection and any hyperparameter tuning
- Discuss any challenges you faced and how you addressed them
- Show your local evaluation results
- Answer questions about your implementation

## Getting Started

1. Download the starter code and data from the course website
2. Install dependencies: `pip install -r requirements.txt`
3. Implement your feature extraction in `feature_extractor.py`
4. Train your models using `train_model.py`
5. Test locally using `evaluate_local.py`
6. Submit to Gradescope: `feature_extractor.py`, `models.pkl`, and optionally `scalers.pkl`

## Files Provided

```
student_starter/
├── feature_extractor.py    # Skeleton code (MODIFY THIS)
├── train_model.py          # Training template
├── evaluate_local.py       # Local evaluation script
├── requirements.txt        # Python dependencies
└── README.md               # Detailed instructions

student_data/
├── train/                  # Training sessions
│   ├── train_00000/
│   │   └── video_traffic.csv
│   └── ...
├── train_labels.csv        # QoE labels for training
└── train_sessions.txt      # List of training session IDs
```



## Tips

- Start with simple features and iterate
- Use cross-validation to avoid overfitting
- Different features may work better for different metrics
- Handle edge cases (empty sessions, missing values)
- Test your code locally before submitting

## Academic Integrity

- You may use any publicly available libraries
- You may discuss approaches with classmates but must write your own code
- Do not share your feature extraction code or trained models
- Attempting to access or derive the hidden test labels is a violation

Good luck!
