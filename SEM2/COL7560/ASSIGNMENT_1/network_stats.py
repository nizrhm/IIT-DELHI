import pandas as pd
import numpy as np

def compute_stats(csv_file):
    df = pd.read_csv(csv_file)

    bw_kbps = df["down_mbps"] * 1000
    lat_ms = df["delay_ms"]

    return (
        int(np.mean(bw_kbps)),
        int(np.std(bw_kbps)),
        int(np.mean(lat_ms)),
        int(np.std(lat_ms))
    )
