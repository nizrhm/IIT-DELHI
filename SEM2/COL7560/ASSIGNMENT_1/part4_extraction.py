import os
import glob
import pandas as pd
import numpy as np
from scapy.all import rdpcap, IP, TCP, UDP
from sklearn.preprocessing import StandardScaler
from sklearn.decomposition import PCA
from sklearn.manifold import TSNE
import matplotlib.pyplot as plt
import seaborn as sns

# --- CONFIGURATION ---
DATA_DIRS = {
    "Web": "web_browsing",
    "ChatGPT": "chatgpt",
    "Video": "video_streaming"
}
OUTPUT_FILE = "final_dataset.csv"

def get_qoe_metric(folder_path, label):
    """Reads the QoE CSV and automatically hunts down the numeric value, ignoring text."""
    try:
        if label == "Web":
            file_path = os.path.join(folder_path, "page_load_time.csv")
        elif label == "ChatGPT":
            file_path = os.path.join(folder_path, "response_time.csv")
        elif label == "Video":
            file_path = os.path.join(folder_path, "player_metrics.csv")
        else:
            return np.nan

        if not os.path.exists(file_path):
            return np.nan

        df = pd.read_csv(file_path)
        
        # 1. First, ask pandas to find columns that are already numbers
        numeric_cols = df.select_dtypes(include=['number']).columns
        if len(numeric_cols) > 0:
            return float(df[numeric_cols[0]].iloc[0])
        
        # 2. Fallback: If it's a messy CSV, check every value in the first row until we find a number
        for val in df.iloc[0]:
            try:
                return float(val)
            except ValueError:
                continue
                
        return np.nan
    except Exception as e:
        return np.nan

def extract_features_from_pcap(pcap_path, label):
    try:
        packets = rdpcap(pcap_path)
    except Exception as e:
        return None

    packet_sizes = []
    timestamps = []
    
    # Note: For a stricter assignment grade, you might want to filter by IP/Port here
    # to isolate ONLY the ChatGPT/Video flows and ignore background OS noise.
    for pkt in packets:
        if IP in pkt:
            packet_sizes.append(len(pkt))
            timestamps.append(float(pkt.time))
            
    if len(packet_sizes) < 2:
        return None

    # Calculate Network Features
    packet_sizes = np.array(packet_sizes)
    timestamps = np.array(timestamps)
    iat = np.diff(timestamps)
    
    duration = timestamps[-1] - timestamps[0]
    total_bytes = np.sum(packet_sizes)
    throughput = total_bytes / duration if duration > 0 else 0

    # Get QoE Metric
    folder_path = os.path.dirname(pcap_path)
    qoe_value = get_qoe_metric(folder_path, label)

    return {
        "label": label,
        "duration": duration,
        "total_packets": len(packet_sizes),
        "total_bytes": total_bytes,
        "throughput_bps": throughput,
        "pkt_mean": np.mean(packet_sizes),
        "pkt_std": np.std(packet_sizes),
        "iat_mean": np.mean(iat) if len(iat)>0 else 0,
        "iat_std": np.std(iat) if len(iat)>0 else 0,
        "qoe_metric": qoe_value # The Application-level performance!
    }

def plot_classification_analysis(df):
    print("[*] Generating PCA and t-SNE plots...")
    # Drop rows with NaN and separate features from labels
    df_clean = df.dropna()
    features = ['duration', 'total_packets', 'total_bytes', 'throughput_bps', 
                'pkt_mean', 'pkt_std', 'iat_mean', 'iat_std']
    
    X = df_clean[features]
    y = df_clean['label']

    # Standardize features (Mean=0, Variance=1) - CRITICAL for PCA/t-SNE
    scaler = StandardScaler()
    X_scaled = scaler.fit_transform(X)

    # PCA
    pca = PCA(n_components=2)
    X_pca = pca.fit_transform(X_scaled)
    
    plt.figure(figsize=(12, 5))
    
    plt.subplot(1, 2, 1)
    sns.scatterplot(x=X_pca[:,0], y=X_pca[:,1], hue=y, palette='Set1')
    plt.title('PCA of Network Traffic')
    plt.xlabel('First Principal Component')
    plt.ylabel('Second Principal Component')

    # t-SNE
    # Perplexity set low (e.g., 15) because dataset is small (150 samples total)
    tsne = TSNE(n_components=2, perplexity=15, random_state=42)
    X_tsne = tsne.fit_transform(X_scaled)

    plt.subplot(1, 2, 2)
    sns.scatterplot(x=X_tsne[:,0], y=X_tsne[:,1], hue=y, palette='Set1')
    plt.title('t-SNE of Network Traffic')
    plt.xlabel('t-SNE Dimension 1')
    plt.ylabel('t-SNE Dimension 2')

    plt.tight_layout()
    plt.savefig('application_classification.png', dpi=300)
    print("[✓] Saved classification plot as 'application_classification.png'")

def plot_qoe_correlation(df):
    print("[*] Generating QoE Correlation Heatmaps...")
    features = ['throughput_bps', 'pkt_mean', 'pkt_std', 'iat_mean', 'iat_std', 'qoe_metric']
    
    # SAFETY NET: Force the column to be numeric, turning any leftover text into 'NaN'
    df['qoe_metric'] = pd.to_numeric(df['qoe_metric'], errors='coerce')
    
    fig, axes = plt.subplots(1, 3, figsize=(18, 5))
    
    for i, (label, folder) in enumerate(DATA_DIRS.items()):
        df_app = df[df['label'] == label][features].dropna()
        if len(df_app) < 2:
            continue
            
        # Calculate Spearman correlation (better for non-linear network relations)
        corr = df_app.corr(method='spearman')
        
        sns.heatmap(corr, annot=True, cmap='coolwarm', vmin=-1, vmax=1, ax=axes[i], fmt=".2f")
        axes[i].set_title(f'{label} QoE Correlation')

    plt.tight_layout()
    plt.savefig('qoe_correlation.png', dpi=300)
    print("[✓] Saved correlation plot as 'qoe_correlation.png'")


def main():
    all_data = []
    print("--- Starting Feature Extraction (This may take a few minutes) ---")
    
    for label, folder in DATA_DIRS.items():
        search_path = os.path.join(folder, "*", "capture.pcap")
        files = glob.glob(search_path)
        print(f"Processing {len(files)} files for {label}...")
        
        for f in files:
            features = extract_features_from_pcap(f, label)
            if features:
                all_data.append(features)
                
    if not all_data:
        print("\n❌ No data found! Ensure folders match 'web_browsing', 'chatgpt', 'video_streaming'.")
        return

    df = pd.DataFrame(all_data)
    df.to_csv(OUTPUT_FILE, index=False)
    print(f"\n✅ Extracted features from {len(df)} traces saved to {OUTPUT_FILE}")

    # --- Run Assignment Analysis ---
    plot_classification_analysis(df)
    plot_qoe_correlation(df)
    print("\n✅ Analysis Complete! You can now use the PNG images in your report.")

if __name__ == "__main__":
    main()