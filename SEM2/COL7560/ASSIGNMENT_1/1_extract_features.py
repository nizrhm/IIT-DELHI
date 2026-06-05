import os
import glob
import pandas as pd
import numpy as np
from scapy.all import rdpcap, TCP, IP

def extract_network_features(pcap_path):
    """Extracts the 6 mandatory features from the largest TCP flow in the PCAP."""
    try:
        packets = rdpcap(pcap_path)
    except Exception as e:
        print(f"Error reading {pcap_path}: {e}")
        return None

    # Filter for TCP/IP packets
    tcp_pkts = [p for p in packets if TCP in p and IP in p]
    if not tcp_pkts: return None

    # Group packets into bi-directional flows
    flows = {}
    for p in tcp_pkts:
        src, dst = p[IP].src, p[IP].dst
        sport, dport = p[TCP].sport, p[TCP].dport
        # Create a unique bi-directional flow key
        flow_id = tuple(sorted([(src, sport), (dst, dport)])) 
        if flow_id not in flows: flows[flow_id] = []
        flows[flow_id].append(p)

    if not flows: return None

    # Heuristic: The application traffic is likely the flow with the most packets
    largest_flow = max(flows.values(), key=len)
    
    # Calculate Features
    times = [float(p.time) for p in largest_flow]
    sizes = [len(p) for p in largest_flow]
    iats = np.diff(times)
    
    duration = times[-1] - times[0] if len(times) > 1 else 0
    throughput = (sum(sizes) * 8) / duration if duration > 0 else 0

    return {
        'flow_duration': duration,
        'flow_throughput': throughput,
        'mean_iat': np.mean(iats) if len(iats) > 0 else 0,
        'std_iat': np.std(iats) if len(iats) > 0 else 0,
        'mean_packet_size': np.mean(sizes),
        'std_packet_size': np.std(sizes)
    }

def process_experiments(base_dir="."):
    dataset = []
    
    # Strictly looking for the 'chatgpt' folder
    apps = {
        "web_browsing": "page_load_time.csv",
        "chatgpt": "response_time.csv", 
        "video_streaming": "player_metrics.csv" 
    }
    
    for app, qoe_file_name in apps.items():
        app_dir = os.path.join(base_dir, app)
        if not os.path.exists(app_dir): 
            continue
            
        print(f"[*] Processing folder: {app_dir}...")
        
        for exp_folder in os.listdir(app_dir):
            exp_path = os.path.join(app_dir, exp_folder)
            if not os.path.isdir(exp_path): continue
                
            pcap_file = os.path.join(exp_path, "capture.pcap")
            qoe_file = os.path.join(exp_path, qoe_file_name)
            
            if not os.path.exists(pcap_file) or not os.path.exists(qoe_file):
                continue
                
            # 1. Extract Network Features
            features = extract_network_features(pcap_file)
            if not features: continue
            
            features['app'] = app
            features['experiment_id'] = exp_folder
            
            # 2. Extract QoE Metrics
            try:
                qoe_df = pd.read_csv(qoe_file)
                if app == "web_browsing":
                    features['qoe_plt'] = float(qoe_df['page_load_time_sec'].iloc[0])
                    
                elif app == "chatgpt": 
                    # MATCHING YOUR EXACT CSV HEADER HERE
                    features['qoe_response_time'] = float(qoe_df['response_time_seconds'].iloc[0])
                    
                elif app == "video_streaming":
                    features['qoe_avg_resolution'] = qoe_df['resolution'].mean()
                    rebuffering_events = (qoe_df['buffer_occupancy'] == 0).sum()
                    features['qoe_rebuffering_ratio'] = rebuffering_events / len(qoe_df)
                    
            except Exception as e:
                print(f"Error reading QoE for {exp_path}: {e}")
                continue
                
            dataset.append(features)
            
    df = pd.DataFrame(dataset)
    df.to_csv("extracted_features.csv", index=False)
    print(f"[*] Extraction complete. Saved {len(df)} rows to extracted_features.csv")

if __name__ == "__main__":
    process_experiments()