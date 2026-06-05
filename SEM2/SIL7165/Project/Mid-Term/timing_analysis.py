import pandas as pd
import numpy as np


class TimingAnalyzer:

    def __init__(self):
        self.records = []

    def record(self, prompt_type, prompt, latency, blocked):

        self.records.append({
            "type": prompt_type,
            "prompt": prompt,
            "latency": latency,
            "blocked": blocked
        })

    def to_dataframe(self):

        return pd.DataFrame(self.records)

    def summary(self, df):

        summary = df.groupby("type")["latency"].agg([
            "mean",
            "std",
            "min",
            "max",
            "count"
        ])

        return summary

    def compute_statistics(self, df):

        safe = df[df["type"] == "safe"]["latency"]
        jailbreak = df[df["type"] == "jailbreak"]["latency"]

        return {
            "safe_mean": np.mean(safe),
            "jailbreak_mean": np.mean(jailbreak),
            "difference": np.mean(jailbreak) - np.mean(safe)
        }