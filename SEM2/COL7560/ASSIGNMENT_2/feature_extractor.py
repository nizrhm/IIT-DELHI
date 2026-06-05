"""
Feature Extractor for Video QoE Prediction

This module contains feature extraction functions for predicting video QoE metrics.
You can implement SEPARATE feature extractors for each QoE metric, allowing you to
design specialized features for each prediction task.

The video_traffic.csv file contains packet-level information:
- timestamp: Unix timestamp of the packet
- ipSrc, ipDst: Source and destination IP addresses
- tcpPortSrc, tcpPortDst: TCP ports (empty if UDP)
- udpPortSrc, udpPortDst: UDP ports (empty if TCP)
- tcpLen, udpLen: Payload length for TCP/UDP packets
- payloadProtocolNumber: 6 for TCP, 17 for UDP

QoE Metrics to Predict:
- avg_resolution: Average video resolution in pixels (144-2160)
- rebuffering_ratio: Fraction of time spent rebuffering (0-1)
- startup_latency: Seconds until playback starts (0+)
- bitrate_switches_per_second: Quality changes per second (0+)

USAGE:
- Implement separate feature extractors for each metric (recommended)
- Or use a single shared extractor via extract_features()
- The autograder will call extract_features() which combines all extractors
"""

import pandas as pd
import numpy as np
from functools import lru_cache
from typing import Dict, Tuple


# =============================================================================
# SHARED PREPROCESSING — read and enrich the CSV exactly once per path
# =============================================================================

@lru_cache(maxsize=64)
def _cached_load(path: str) -> Tuple[pd.DataFrame, pd.Series]:
    """
    Read, parse, and precompute ALL shared columns once per unique path.

    Cached so that the four metric-specific extractors called from
    extract_features() pay the IO + compute cost only once.

    Returns:
        (df, throughput_window)  — both are the canonical copies stored in
        the cache; callers must copy them before mutating.
    """
    df = pd.read_csv(path)



    df["timestamp_readable"] = pd.to_datetime(df["timestamp"], unit="s")

    df.sort_values("timestamp_readable", inplace=True)
    df.reset_index(drop=True, inplace=True)

    # Shared derived columns
    df["pkt_size"]  = df["tcpLen"].fillna(0) + df["udpLen"].fillna(0)
    df["time_diff"] = df["timestamp_readable"].diff().dt.total_seconds().fillna(0)
    df["t_rel"]     = (df["timestamp_readable"] - df["timestamp_readable"].iloc[0]).dt.total_seconds()
    df["window"]    = df["timestamp_readable"].dt.floor("10s")
    df["cum_bytes"] = df["pkt_size"].cumsum()

    # Window-level throughput (Mbps) — used by both resolution & switches
    window_bytes      = df.groupby("window")["pkt_size"].sum()
    throughput_window = (window_bytes * 8) / (10 * 1e6)

    return df, throughput_window


def _load(path: str) -> Tuple[pd.DataFrame, pd.Series]:
    """Return *copies* of the cached frame and window-throughput series."""
    df, thr = _cached_load(path)
    return df.copy(), thr.copy()


# =============================================================================
# METRIC-SPECIFIC FEATURE EXTRACTORS
# =============================================================================

def extract_features_resolution(video_traffic_path: str) -> Dict[str, float]:
    """
    Extract features for predicting AVERAGE RESOLUTION.

    Resolution is typically correlated with:
    - Overall throughput (higher throughput = higher resolution possible)
    - Sustained bandwidth over time
    - Packet sizes (larger packets often indicate higher quality video)

    Args:
        video_traffic_path: Path to the video_traffic.csv file

    Returns:
        Dictionary mapping feature names to float values
    """
    df, throughput_window = _load(video_traffic_path)# ===============================
# BURST CREATION
# ===============================

    # Ensure sorting
# ===============================
# 1. PAYLOAD PROCESSING
# ===============================

    df[["tcpLen", "udpLen"]] = df[["tcpLen", "udpLen"]].fillna(0)
    df["total_len"] = df["tcpLen"] + df["udpLen"]

    # Filter small packets
    df = df[df["total_len"] >= 100].reset_index(drop=True)

    # ===============================
    # 2. TARGET IP SELECTION (SAFE)
    # ===============================

    ips = df.loc[df['ipSrc'].str.startswith('10.', na=False), 'ipSrc'].unique()
    target_ip = ips[0] if len(ips) > 0 else df["ipSrc"].iloc[0]

    # Masks
    dwn_msk = df["ipDst"] == target_ip
    up_msk  = df["ipSrc"] == target_ip

    # ===============================
    # 3. DIRECTION + BURST TRACKING
    # ===============================

    df["direction"] = (df["ipSrc"] == target_ip).astype(int)

    # 🔥 IMPROVED burst logic (important)
    df = df.sort_values("timestamp").reset_index(drop=True)
    df["iat"] = df["timestamp"].diff().fillna(0)

    threshold = 0.05  # 50 ms
    df["new_burst"] = (
        (df["direction"] != df["direction"].shift()) 
        # |        (df["iat"] > threshold)
    )

    df["direction_id"] = df["new_burst"].cumsum()

    # ===============================
    # 4. PROTOCOL FLAGS
    # ===============================

    df["tcp"] = (df["tcpLen"] > 0).astype(int)
    df["udp"] = (df["udpLen"] > 0).astype(int)

    # ===============================
    # 5. TIMESTAMP + LENGTH SPLIT
    # ===============================

    df["timestamp_readable"] = pd.to_datetime(df["timestamp"], unit="s")
    df = df.sort_values("timestamp_readable").reset_index(drop=True)

    df["uplink_len"] = df["total_len"].where(up_msk, 0)
    df["downlink_len"] = df["total_len"].where(dwn_msk, 0)

    # Packet size
    df["pkt_size"] = df["uplink_len"] + df["downlink_len"]

    # ===============================
    # 6. BURST AGGREGATION
    # ===============================

    bursty = df.groupby("direction_id").agg({
        "total_len": "sum",
        "tcp": "max",
        "udp": "max",
        "uplink_len": "sum",
        "downlink_len": "sum",
        "timestamp_readable": ["first", "last"],
    })

    bursty.columns = [
        'total_bytes','is_tcp','is_udp',
        "uplink_len","downlink_len",
        'start_time',"endtime"
    ]

    # ===============================
    # 7. DURATION
    # ===============================

    bursty["duration"] = (
        (bursty["endtime"] - bursty["start_time"])
        .dt.total_seconds()
        .clip(lower=1e-3)
    )

    # ===============================
    # 8. THROUGHPUT
    # ===============================

    bursty['burst_throughput_Mbps'] = (bursty['total_bytes'] * 8) / (bursty["duration"] * 1e6)
    bursty['burst_up_throughput_Mbps'] = (bursty['uplink_len'] * 8) / (bursty["duration"] * 1e6)
    bursty['burst_dwn_throughput_Mbps'] = (bursty['downlink_len'] * 8) / (bursty["duration"] * 1e6)

    # Extra
    # bursty["is_mixed"] = ((bursty["is_tcp"] == 1) & (bursty["is_udp"] == 1)).astype(int)
    bursty["pkt_density"] = bursty["total_bytes"] / (bursty["duration"] + 1e-6)

    # ===============================
    # 9. FEATURE EXTRACTION
    # ===============================

    features = {}

    packet_size = df["pkt_size"]

    # Basic stats
    features["pkt_size_mean"] = packet_size.mean()
    features["pkt_size_std"] = packet_size.std()
    features["pkt_size_ratio"] = (packet_size > packet_size.mean()).sum() / len(packet_size)

    # Session throughput
    time_duration = (
        df["timestamp_readable"].iloc[-1] - df["timestamp_readable"].iloc[0]
    ).total_seconds()
    time_duration = max(time_duration, 1e-6)

    features["throughput_Mbps"] = (packet_size.sum() * 8) / (time_duration * 1e6)

    # Burst stats
    features["burst_count"] = len(bursty)
    features["burst_duration_mean"] = bursty["duration"].mean()
    features["burst_duration_std"] = bursty["duration"].std()

    features["burst_throughput_mean"] = bursty["burst_throughput_Mbps"].mean()
    features["burst_throughput_std"] = bursty["burst_throughput_Mbps"].std()
    features["burst_throughput_p90"] = bursty["burst_throughput_Mbps"].quantile(0.9)

    # Uplink/downlink
    uplink_total = bursty["uplink_len"].sum()
    downlink_total = bursty["downlink_len"].sum()

    features["uplink_ratio"] = uplink_total / (uplink_total + downlink_total + 1e-6)
    features["uplink_mean"]=bursty["uplink_len"].mean()
    features["downlink_mean"]=bursty["downlink_len"].mean()
    features["uplink_std"]=bursty["uplink_len"].std()
    features["downlink_std"]=bursty["downlink_len"].std()
    
    # Burst gaps (🔥 VERY IMPORTANT)
    bursty = bursty.sort_values("start_time")
    burst_gap = bursty["start_time"].diff().dt.total_seconds().fillna(0)

    features["burst_gap_mean"] = burst_gap.mean()
    features["burst_gap_std"] = burst_gap.std()

    # IAT
    iat = df["timestamp_readable"].diff().dt.total_seconds().fillna(0)

    features["iat_mean"] = iat.mean()
    features["iat_std"] = iat.std()
    features["iat_p90"] = iat.quantile(0.9)

    # Protocol ratios
    features["tcp_ratio"] = bursty["is_tcp"].mean()
    features["udp_ratio"] = bursty["is_udp"].mean()
    # features["mixed_ratio"] = bursty["is_mixed"].mean()

    # Density
    features["bursts_per_sec"] = len(bursty) / (time_duration + 1e-6)
    return features


def extract_features_rebuffering(video_traffic_path: str) -> Dict[str, float]:
    """
    Extract features for predicting REBUFFERING RATIO.

    Rebuffering is typically correlated with:
    - Throughput variability and drops
    - Idle periods (gaps in packet arrivals)
    - Buffer depletion patterns
    - Periods of low bandwidth

    Args:
        video_traffic_path: Path to the video_traffic.csv file

    Returns:
        Dictionary mapping feature names to float values
    """
    df, _ = _load(video_traffic_path)

##############

    # Ensure sorting
# ===============================
# 1. PAYLOAD PROCESSING
# ===============================

    df[["tcpLen", "udpLen"]] = df[["tcpLen", "udpLen"]].fillna(0)
    df["total_len"] = df["tcpLen"] + df["udpLen"]

    # Filter small packets
    df = df[df["total_len"] >= 100].reset_index(drop=True)

    # ===============================
    # 2. TARGET IP SELECTION (SAFE)
    # ===============================

    ips = df.loc[df['ipSrc'].str.startswith('10.', na=False), 'ipSrc'].unique()
    target_ip = ips[0] if len(ips) > 0 else df["ipSrc"].iloc[0]

    # Masks
    dwn_msk = df["ipDst"] == target_ip
    up_msk  = df["ipSrc"] == target_ip

    # ===============================
    # 3. DIRECTION + BURST TRACKING
    # ===============================

    df["direction"] = (df["ipSrc"] == target_ip).astype(int)

    # 🔥 IMPROVED burst logic (important)
    df = df.sort_values("timestamp").reset_index(drop=True)
    df["iat"] = df["timestamp"].diff().fillna(0)

    threshold = 0.05  # 50 ms
    df["new_burst"] = (
        (df["direction"] != df["direction"].shift()) 
        # |        (df["iat"] > threshold)
    )

    df["direction_id"] = df["new_burst"].cumsum()

    # ===============================
    # 4. PROTOCOL FLAGS
    # ===============================

    df["tcp"] = (df["tcpLen"] > 0).astype(int)
    df["udp"] = (df["udpLen"] > 0).astype(int)

    # ===============================
    # 5. TIMESTAMP + LENGTH SPLIT
    # ===============================

    df["timestamp_readable"] = pd.to_datetime(df["timestamp"], unit="s")
    df = df.sort_values("timestamp_readable").reset_index(drop=True)

    df["uplink_len"] = df["total_len"].where(up_msk, 0)
    df["downlink_len"] = df["total_len"].where(dwn_msk, 0)


    # Packet size
    df["pkt_size"] = df["uplink_len"] + df["downlink_len"]

    # ===============================
    # 6. BURST AGGREGATION
    # ===============================

    bursty = df.groupby("direction_id").agg({
        "total_len": "sum",
        "tcp": "max",
        "udp": "max",
        "uplink_len": "sum",
        "downlink_len": "sum",
        "timestamp_readable": ["first", "last"],
    })

    bursty.columns = [
        'total_bytes','is_tcp','is_udp',
        "uplink_len","downlink_len",
        'start_time',"endtime"
    ]

    # ===============================
    # 7. DURATION
    # ===============================

    bursty["duration"] = (
        (bursty["endtime"] - bursty["start_time"])
        .dt.total_seconds()
        .clip(lower=1e-3)
    )

    # ===============================
    # 8. THROUGHPUT
    # ===============================

    bursty['burst_throughput_Mbps'] = (bursty['total_bytes'] * 8) / (bursty["duration"] * 1e6)
    bursty['burst_up_throughput_Mbps'] = (bursty['uplink_len'] * 8) / (bursty["duration"] * 1e6)
    bursty['burst_dwn_throughput_Mbps'] = (bursty['downlink_len'] * 8) / (bursty["duration"] * 1e6)

    bursty["downlink_time"] = (
        (bursty["endtime"] - bursty["start_time"])
        .dt.total_seconds()
        .where(bursty["burst_dwn_throughput_Mbps"] > 0
            #    ,0
            )
    )
    bursty["uplink_time"] = (
        (bursty["endtime"] - bursty["start_time"])
        .dt.total_seconds()
        .where(bursty["burst_up_throughput_Mbps"] > 0
            #    ,0
            )
    )
    # Extra
    # bursty["is_mixed"] = ((bursty["is_tcp"] == 1) & (bursty["is_udp"] == 1)).astype(int)
    bursty["pkt_density"] = bursty["total_bytes"] / (bursty["duration"] + 1e-6)

    # ===============================
    # 9. FEATURE EXTRACTION
    # ===============================

    features = {}

    packet_size = df["pkt_size"]

    # Basic stats
    features["dwon_chunk_time_mean"]=bursty["downlink_time"].mean()
    features["dwon_chunk_time_std"]=bursty["downlink_time"].std()
    features["dwon_chunk_time_90p"] = bursty["downlink_time"].quantile(0.9)
    features["up_chunk_time_mean"]=bursty["uplink_time"].mean()
    features["up_chunk_time_std"]=bursty["uplink_time"].std()
    features["up_chunk_time_90p"] = bursty["uplink_time"].quantile(0.9)


    features["pkt_size_mean"] = packet_size.mean()
    features["pkt_size_std"] = packet_size.std()
    features["pkt_size_ratio"] = (packet_size > packet_size.mean()).sum() / len(packet_size)

    # Session throughput
    time_duration = (
        df["timestamp_readable"].iloc[-1] - df["timestamp_readable"].iloc[0]
    ).total_seconds()
    time_duration = max(time_duration, 1e-6)

    features["throughput_Mbps"] = (packet_size.sum() * 8) / (time_duration * 1e6)

    # Burst stats
    features["burst_count"] = len(bursty)
    features["burst_duration_mean"] = bursty["duration"].mean()
    features["burst_duration_std"] = bursty["duration"].std()

    features["burst_throughput_mean"] = bursty["burst_throughput_Mbps"].mean()
    features["burst_throughput_std"] = bursty["burst_throughput_Mbps"].std()
    features["burst_throughput_p90"] = bursty["burst_throughput_Mbps"].quantile(0.9)

    # Uplink/downlink
    uplink_total = bursty["uplink_len"].sum()
    downlink_total = bursty["downlink_len"].sum()

    features["uplink_ratio"] = uplink_total / (uplink_total + downlink_total + 1e-6)
    features["uplink_mean"]=bursty["uplink_len"].mean()
    features["downlink_mean"]=bursty["downlink_len"].mean()
    features["uplink_std"]=bursty["uplink_len"].std()
    features["downlink_std"]=bursty["downlink_len"].std()
    
    # Burst gaps (🔥 VERY IMPORTANT)
    bursty = bursty.sort_values("start_time")
    burst_gap = bursty["start_time"].diff().dt.total_seconds().fillna(0)

    features["burst_gap_mean"] = burst_gap.mean()
    features["burst_gap_std"] = burst_gap.std()

    # IAT
    iat = df["timestamp_readable"].diff().dt.total_seconds().fillna(0)

    features["iat_mean"] = iat.mean()
    features["iat_std"] = iat.std()
    features["iat_p90"] = iat.quantile(0.9)

    # Protocol ratios
    features["tcp_ratio"] = bursty["is_tcp"].mean()
    features["udp_ratio"] = bursty["is_udp"].mean()
    # features["mixed_ratio"] = bursty["is_mixed"].mean()

    # Density
    features["bursts_per_sec"] = len(bursty) / (time_duration + 1e-6)
#################


    threshold  = 1
    idle       = df["time_diff"] > threshold
    total_time = max(df["t_rel"].iloc[-1], 1e-6)

    # Corrected idle time (subtract the threshold so normal inter-packet gaps
    # don't inflate the count)
    idle_time = (df.loc[idle, "time_diff"] - threshold).sum()

    features["idle_time_ratio"]     = idle_time / total_time
    features["num_idle"]            = idle.sum()
    features["idle_freq"]           = idle.sum() / total_time
    features["mean_idle_duration"]  = idle_time / (idle.sum() + 1e-6)
    features["max_idle_gap"]        = df["time_diff"].max()
    features["time_diff_std"]       = df["time_diff"].std()
    features["long_idle_ratio"]     = (df["time_diff"] > 3).mean()

    return features


def extract_features_startup(video_traffic_path: str) -> Dict[str, float]:
    """
    Extract features for predicting STARTUP LATENCY.

    Startup latency is typically correlated with:
    - Time to receive initial data burst
    - Early packet timing patterns
    - Initial throughput ramp-up
    - Time to fill initial buffer

    Args:
        video_traffic_path: Path to the video_traffic.csv file

    Returns:
        Dictionary mapping feature names to float values
    """
    df, _ = _load(video_traffic_path)
    features = {}

    # 1. Time to first burst (first moment cumulative bytes exceed 100 KB)
    burst = df[df["cum_bytes"] > 100 * 1024]
    features["time_to_first_burst"] = burst["t_rel"].iloc[0] if len(burst) > 0 else 0

    # 2. Bytes / rate in first N seconds
    N            = 30
    mask_N       = df["t_rel"] <= N
    bytes_first  = df.loc[mask_N, "pkt_size"].sum()
    features["bytes_first_5s"]      = bytes_first
    features["bytes_rate_first_5s"] = bytes_first / N

    # 3. Initial throughput (first 5 s)
    init_mask = df["t_rel"] <= 5
    duration  = max(df.loc[init_mask, "t_rel"].max(), 1e-6)
    features["initial_throughput_Mbps"] = (df.loc[init_mask, "pkt_size"].sum() * 8) / (duration * 1e6)

    # 4. Ramp-up speed (slope of per-2s throughput over first 10 s)
    early        = df[df["t_rel"] <= 10].copy()
    early["win"] = (early["t_rel"] // 2).astype(int)
    wbytes       = early.groupby("win")["pkt_size"].sum()
    thr          = (wbytes * 8) / (2 * 1e6)
    features["ramp_up_speed"] = thr.iloc[-1] - thr.iloc[0] if len(thr) > 1 else 0

    return features


def extract_features_switches(video_traffic_path: str) -> Dict[str, float]:
    """
    Extract features for predicting BITRATE SWITCHES PER SECOND.

    Bitrate switches are typically correlated with:
    - Throughput variability over time
    - Coefficient of variation of bandwidth
    - Frequency of throughput changes
    - Network instability

    Args:
        video_traffic_path: Path to the video_traffic.csv file

    Returns:
        Dictionary mapping feature names to float values
    """
    _, thr = _load(video_traffic_path)
    features = {}

    mean_thr = thr.mean()
    std_thr  = thr.std()

    # 1. Coefficient of variation
    features["throughput_cv"] = std_thr / (mean_thr + 1e-6)

    # 2. Number of large throughput swings (>30 % relative change)
    thr_pct_change = thr.pct_change().abs()
    features["num_throughput_changes"] = (thr_pct_change > 0.3).sum()

    # 3. Stability metrics
    thr_diff = thr.diff().abs()
    features["throughput_std"]  = std_thr
    features["mean_thr_change"] = thr_diff.mean()
    features["max_thr_jump"]    = thr_diff.max()

    # 4. Direction-change (oscillation) count
    diff = thr.diff()
    features["oscillation_count"] = ((diff.shift(1) * diff) < 0).sum()

    # 5. Stability score
    features["stability_score"] = 1 / (1 + features["throughput_cv"])

    return features


# =============================================================================
# MAIN FEATURE EXTRACTOR
# This function combines all metric-specific extractors
# =============================================================================

def extract_features(video_traffic_path: str) -> Dict[str, float]:
    """
    Extract ALL features for a session (combines all metric-specific extractors).

    This function is called by the autograder. It combines features from all
    metric-specific extractors into a single feature dictionary.

    Args:
        video_traffic_path: Path to the video_traffic.csv file

    Returns:
        Dictionary mapping feature names to float values
    """
    features = {}

    # Combine features from all extractors with prefixes to avoid name collisions
    for prefix, extractor in [
        ('res',    extract_features_resolution),
        ('rebuf',  extract_features_rebuffering),
        ('start',  extract_features_startup),
        ('switch', extract_features_switches),
    ]:
        try:
            metric_features = extractor(video_traffic_path)
            for name, value in metric_features.items():
                features[f'{prefix}_{name}'] = value
        except Exception as e:
            print(f"Warning: {prefix} extractor failed: {e}")

    return features


# =============================================================================
# HELPER FUNCTIONS
# =============================================================================

def extract_features_for_all_sessions(
    data_dir: str,
    sessions_file: str,
    output_path: str = None
) -> pd.DataFrame:
    """
    Extract features for all sessions listed in sessions_file.

    Args:
        data_dir: Directory containing session subdirectories
        sessions_file: Path to file listing session IDs (one per line)
        output_path: Optional path to save features CSV

    Returns:
        DataFrame with session_id and extracted features
    """
    from pathlib import Path

    data_path = Path(data_dir)

    # Read session IDs
    with open(sessions_file, 'r') as f:
        session_ids = [line.strip() for line in f if line.strip()]

    all_features = []

    print(f"Extracting features for {len(session_ids)} sessions...")

    for i, session_id in enumerate(session_ids):
        video_traffic_path = data_path / session_id / 'video_traffic.csv'

        if not video_traffic_path.exists():
            print(f"Warning: {video_traffic_path} not found, skipping...")
            continue

        try:
            features = extract_features(str(video_traffic_path))
            features['session_id'] = session_id
            all_features.append(features)
        except Exception as e:
            print(f"Error processing {session_id}: {e}")
            continue

        if (i + 1) % 500 == 0:
            print(f"  Processed {i + 1}/{len(session_ids)} sessions...")

    df = pd.DataFrame(all_features)

    # Move session_id to first column
    cols = ['session_id'] + [c for c in df.columns if c != 'session_id']
    df = df[cols]

    if output_path:
        df.to_csv(output_path, index=False)
        print(f"Saved features to {output_path}")

    return df


if __name__ == '__main__':
    # Test on a single session
    import sys

    if len(sys.argv) > 1:
        test_path = sys.argv[1]
    else:
        test_path = 'data/train/train_00000/video_traffic.csv'

    print(f"Testing feature extraction on: {test_path}")
    print("=" * 60)

    try:
        features = extract_features(test_path)
        print(f"\nExtracted {len(features)} features:")
        for name, value in sorted(features.items()):
            print(f"  {name}: {value:.4f}")
    except FileNotFoundError:
        print(f"File not found: {test_path}")
        print("Usage: python feature_extractor.py <path_to_video_traffic.csv>")