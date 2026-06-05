import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.decomposition import PCA
from sklearn.manifold import TSNE
from sklearn.preprocessing import StandardScaler
import os

def load_data(file_path="extracted_features.csv"):
    if not os.path.exists(file_path):
        print(f"[!] Error: {file_path} not found. Run 1_extract_features.py first.")
        return None
    return pd.read_csv(file_path)

def plot_classification(df):
    """Performs PCA and t-SNE and plots the feature separability."""
    print("[*] Generating Classification Plots (PCA & t-SNE)...")
    
    # Select only the network features for clustering
    features = ['flow_duration', 'flow_throughput', 'mean_iat', 'std_iat', 'mean_packet_size', 'std_packet_size']
    
    # Drop rows with missing values to avoid errors
    df_clean = df.dropna(subset=features)
    
    # Standardize features (mean=0, std=1)
    x = StandardScaler().fit_transform(df_clean[features])
    
    # Perform PCA
    pca = PCA(n_components=2)
    pca_result = pca.fit_transform(x)
    
    # Perform t-SNE
    tsne = TSNE(n_components=2, perplexity=30, random_state=42)
    tsne_result = tsne.fit_transform(x)
    
    # Plotting
    fig, axes = plt.subplots(1, 2, figsize=(16, 7))
    
    # PCA Plot
    sns.scatterplot(x=pca_result[:,0], y=pca_result[:,1], hue=df_clean['app'], ax=axes[0], palette="bright", s=60)
    axes[0].set_title('PCA of Network Features')
    axes[0].grid(True, linestyle='--', alpha=0.6)
    
    # t-SNE Plot
    sns.scatterplot(x=tsne_result[:,0], y=tsne_result[:,1], hue=df_clean['app'], ax=axes[1], palette="bright", s=60)
    axes[1].set_title('t-SNE of Network Features')
    axes[1].grid(True, linestyle='--', alpha=0.6)
    
    plt.tight_layout()
    plt.savefig('classification_plots.png', dpi=300)
    print("[*] Saved classification_plots.png")

def plot_correlations(df):
    """Calculates and plots Pearson correlations for each app's specific QoE metrics."""
    print("[*] Generating Correlation Heatmaps...")
    
    features = ['flow_duration', 'flow_throughput', 'mean_iat', 'std_iat', 'mean_packet_size', 'std_packet_size']
    
    # --- 1. Web Browsing Correlation (QoE = PLT) ---
    web_df = df[df['app'] == 'web_browsing'].copy()
    if not web_df.empty:
        # Calculate correlation matrix
        corr = web_df[features + ['qoe_plt']].corr()
        # Isolate just the QoE column for the heatmap
        target_corr = corr[['qoe_plt']].drop('qoe_plt')
        plot_heatmap(target_corr, 'Web Browsing (Page Load Time)', 'corr_web.png')
    else:
        print("[!] Warning: No Web Browsing data found.")

    # --- 2. ChatGPT Correlation (QoE = Response Time) ---
    # Note: We look for 'chatgpt' because that is what 1_extract_features.py saved.
    chat_df = df[df['app'] == 'chatgpt'].copy()
    if not chat_df.empty:
        corr = chat_df[features + ['qoe_response_time']].corr()
        target_corr = corr[['qoe_response_time']].drop('qoe_response_time')
        plot_heatmap(target_corr, 'ChatGPT (Response Time)', 'corr_chatgpt.png')
    else:
        print("[!] Warning: No ChatGPT data found.")

    # --- 3. Video Streaming Correlation (QoE = Avg Resolution & Rebuffering) ---
    video_df = df[df['app'] == 'video_streaming'].copy()
    if not video_df.empty:
        # We have 2 QoE metrics here
        corr = video_df[features + ['qoe_avg_resolution', 'qoe_rebuffering_ratio']].corr()
        # Keep only the correlations between Features (rows) and QoE Metrics (cols)
        target_corr = corr[['qoe_avg_resolution', 'qoe_rebuffering_ratio']].loc[features]
        plot_heatmap(target_corr, 'Video Streaming (Res & Rebuffer)', 'corr_video.png')
    else:
        print("[!] Warning: No Video Streaming data found.")

def plot_heatmap(corr_data, title, filename):
    plt.figure(figsize=(8, 6))
    sns.heatmap(corr_data, annot=True, cmap='RdBu_r', vmin=-1, vmax=1, fmt=".2f", linewidths=.5)
    plt.title(f"Correlation: {title}")
    plt.tight_layout()
    plt.savefig(filename, dpi=300)
    print(f"[*] Saved {filename}")

if __name__ == "__main__":
    df = load_data()
    if df is not None:
        plot_classification(df)
        plot_correlations(df)