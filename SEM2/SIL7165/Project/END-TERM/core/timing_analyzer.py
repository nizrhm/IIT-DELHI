"""
Timing Analyzer
================
Utility for measuring Inter-Token Latency (ITL) and computing
statistical outlier scores (Z-Score) to detect side-channel leaks.
"""

import time
import numpy as np


class TimingAnalyzer:
    """Consumes a token stream and measures the ITL distribution."""

    def __init__(self):
        self.itls = []

    def consume_stream(self, generator):
        """Drains a generator while recording inter-token latencies."""
        self.itls = []
        last_time = None
        for token in generator:
            current_time = time.time()
            if last_time is not None:
                self.itls.append(current_time - last_time)
            last_time = current_time
        return self.itls

    def compute_z_score(self):
        """
        Computes the Z-Score of the maximum ITL spike.
        A Z-Score > 3.0 indicates a statistically significant outlier,
        meaning an attacker can identify the moderation stall.
        """
        if len(self.itls) < 2:
            return 0.0
        mean_itl = np.mean(self.itls)
        std_itl = np.std(self.itls)
        if std_itl < 0.01:
            return 0.0  # Variance is virtually zero - no detectable spikes
        return (np.max(self.itls) - mean_itl) / std_itl

    def get_max_spike(self):
        return max(self.itls) if self.itls else 0.0

    def get_summary(self):
        """Returns a dict with key metrics for logging."""
        return {
            "samples": len(self.itls),
            "max_spike": f"{self.get_max_spike():.3f}s",
            "z_score": f"{self.compute_z_score():.2f}",
            "mean_itl": f"{np.mean(self.itls):.3f}s" if self.itls else "N/A",
        }
