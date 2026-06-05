#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from datetime import datetime
import glob
import os
import numpy as np

# Set style for better plots
plt.style.use('seaborn-v0_8')
sns.set_palette("husl")

def parse_health_log(filename):
    """Parse health check log file"""
    df = pd.read_csv(filename, names=['timestamp', 'server_id', 'response_time', 'status'])
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    df['minute_rounded'] = df['timestamp'].dt.floor('min')
    return df

def parse_forward_log(filename):
    """Parse forward log file"""
    df = pd.read_csv(filename, names=['timestamp', 'action', 'server_id', 'operation'])
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    df['minute_rounded'] = df['timestamp'].dt.floor('min')
    
    # Extract operation type and filename
    df[['op_type', 'filename']] = df['operation'].str.split(' ', n=1, expand=True)
    return df

def create_health_plots(health_df, output_dir='plots'):
    """Create health check analysis plots"""
    os.makedirs(output_dir, exist_ok=True)
    
    # Plot 1: Server Availability Over Time
    plt.figure(figsize=(12, 6))
    
    # Convert status to numeric for plotting
    health_df['status_numeric'] = health_df['status'].map({'UP': 1, 'DOWN': 0})
    
    # Create pivot table for heatmap
    pivot_data = health_df.pivot_table(
        index='timestamp', 
        columns='server_id', 
        values='status_numeric', 
        aggfunc='first'
    ).fillna(0)
    
    plt.subplot(1, 2, 1)
    plt.imshow(pivot_data.T, aspect='auto', cmap='RdYlGn', interpolation='nearest')
    plt.colorbar(label='Status (1=UP, 0=DOWN)')
    plt.title('Server Status Over Time')
    plt.xlabel('Time Index')
    plt.ylabel('Server ID')
    plt.yticks(range(len(pivot_data.columns)), pivot_data.columns)
    
    # Plot 2: Response Time Distribution
    plt.subplot(1, 2, 2)
    healthy_df = health_df[health_df['status'] == 'UP']
    for server_id in healthy_df['server_id'].unique():
        server_data = healthy_df[healthy_df['server_id'] == server_id]
        plt.plot(server_data['timestamp'], server_data['response_time'], 
                label=f'Server {server_id}', marker='o', markersize=2, linewidth=1)
    
    plt.title('Response Time Over Time (Healthy Servers)')
    plt.xlabel('Time')
    plt.ylabel('Response Time (ms)')
    plt.legend()
    plt.xticks(rotation=45)
    plt.tight_layout()
    
    plt.savefig(f'{output_dir}/health_overview.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # Plot 3: Server Uptime Statistics
    plt.figure(figsize=(10, 6))
    
    uptime_stats = health_df.groupby('server_id')['status'].apply(
        lambda x: (x == 'UP').sum() / len(x) * 100
    ).reset_index()
    uptime_stats.columns = ['server_id', 'uptime_percentage']
    
    plt.subplot(1, 2, 1)
    plt.bar(uptime_stats['server_id'], uptime_stats['uptime_percentage'], color='skyblue')
    plt.title('Server Uptime Percentage')
    plt.xlabel('Server ID')
    plt.ylabel('Uptime (%)')
    plt.ylim(0, 100)
    
    for i, v in enumerate(uptime_stats['uptime_percentage']):
        plt.text(i, v + 1, f'{v:.1f}%', ha='center', va='bottom')
    
    # Plot 4: Response Time Statistics
    plt.subplot(1, 2, 2)
    response_stats = healthy_df.groupby('server_id')['response_time'].agg(['mean', 'std', 'min', 'max']).reset_index()
    
    x_pos = np.arange(len(response_stats))
    plt.bar(x_pos - 0.2, response_stats['mean'], 0.4, label='Mean', alpha=0.8)
    plt.bar(x_pos + 0.2, response_stats['max'], 0.4, label='Max', alpha=0.8)
    
    plt.title('Response Time Statistics (Healthy Only)')
    plt.xlabel('Server ID')
    plt.ylabel('Response Time (ms)')
    plt.xticks(x_pos, response_stats['server_id'])
    plt.legend()
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/health_statistics.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # Plot 5: Health Check Frequency
    plt.figure(figsize=(10, 6))
    checks_per_minute = health_df.groupby(['minute_rounded', 'server_id']).size().unstack(fill_value=0)
    
    checks_per_minute.plot(kind='bar', stacked=True, figsize=(12, 6))
    plt.title('Health Checks Per Minute')
    plt.xlabel('Time (Minutes)')
    plt.ylabel('Number of Health Checks')
    plt.legend(title='Server ID')
    plt.xticks(rotation=45)
    plt.tight_layout()
    
    plt.savefig(f'{output_dir}/health_frequency.png', dpi=300, bbox_inches='tight')
    plt.close()

def create_forward_plots(forward_df, output_dir='plots'):
    """Create request forwarding analysis plots"""
    os.makedirs(output_dir, exist_ok=True)
    
    # Plot 1: Request Types Over Time
    plt.figure(figsize=(12, 6))
    
    # Count requests by type and time
    request_counts = forward_df[forward_df['action'] == 'ARRIVE'].groupby(
        ['minute_rounded', 'op_type']
    ).size().unstack(fill_value=0)
    
    request_counts.plot(kind='bar', stacked=True, figsize=(12, 6))
    plt.title('Request Arrivals Over Time by Type')
    plt.xlabel('Time (Minutes)')
    plt.ylabel('Number of Requests')
    plt.legend(title='Operation Type')
    plt.xticks(rotation=45)
    plt.tight_layout()
    
    plt.savefig(f'{output_dir}/request_types.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # Plot 2: Server Load Distribution
    plt.figure(figsize=(12, 6))
    
    forward_actions = forward_df[forward_df['action'] == 'FORWARD']
    server_load = forward_actions.groupby(['server_id', 'op_type']).size().unstack(fill_value=0)
    
    plt.subplot(1, 2, 1)
    server_load.plot(kind='bar', stacked=True, ax=plt.gca())
    plt.title('Server Load Distribution by Operation Type')
    plt.xlabel('Server ID')
    plt.ylabel('Number of Requests')
    plt.legend(title='Operation Type')
    
    # Plot 3: Request Rate Over Time
    plt.subplot(1, 2, 2)
    request_rate = forward_df[forward_df['action'] == 'ARRIVE'].groupby('minute_rounded').size()
    request_rate.plot(kind='line', marker='o')
    plt.title('Request Arrival Rate Over Time')
    plt.xlabel('Time (Minutes)')
    plt.ylabel('Requests per Minute')
    plt.xticks(rotation=45)
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/server_load.png', dpi=300, bbox_inches='tight')
    plt.close()
    
    # Plot 4: Operation Type Distribution
    plt.figure(figsize=(10, 6))
    
    op_distribution = forward_df[forward_df['action'] == 'ARRIVE']['op_type'].value_counts()
    plt.pie(op_distribution.values, labels=op_distribution.index, autopct='%1.1f%%', startangle=90)
    plt.title('Distribution of Operation Types')
    
    plt.savefig(f'{output_dir}/operation_distribution.png', dpi=300, bbox_inches='tight')
    plt.close()

def create_comparison_plots(health_dfs, forward_dfs, output_dir='plots'):
    """Create comparison plots between different algorithm runs"""
    os.makedirs(output_dir, exist_ok=True)
    
    # Compare response times between algorithms
    plt.figure(figsize=(12, 8))
    
    for i, (algo_name, health_df) in enumerate(health_dfs.items()):
        healthy_df = health_df[health_df['status'] == 'UP']
        response_times = healthy_df.groupby('server_id')['response_time'].mean()
        
        plt.subplot(2, 2, i + 1)
        plt.bar(response_times.index, response_times.values, color=f'C{i}', alpha=0.7)
        plt.title(f'{algo_name} - Average Response Times')
        plt.xlabel('Server ID')
        plt.ylabel('Average Response Time (ms)')
        
        for j, v in enumerate(response_times.values):
            plt.text(j, v + 0.1, f'{v:.1f}ms', ha='center', va='bottom')
    
    # Compare server utilization
    for i, (algo_name, forward_df) in enumerate(forward_dfs.items()):
        forward_actions = forward_df[forward_df['action'] == 'FORWARD']
        server_utilization = forward_actions['server_id'].value_counts().sort_index()
        
        plt.subplot(2, 2, i + 3)
        plt.bar(server_utilization.index, server_utilization.values, color=f'C{i}', alpha=0.7)
        plt.title(f'{algo_name} - Request Distribution')
        plt.xlabel('Server ID')
        plt.ylabel('Number of Requests')
        
        for j, v in enumerate(server_utilization.values):
            plt.text(j, v + 0.1, str(v), ha='center', va='bottom')
    
    plt.tight_layout()
    plt.savefig(f'{output_dir}/algorithm_comparison.png', dpi=300, bbox_inches='tight')
    plt.close()

def generate_report(health_df, forward_df, output_dir='plots'):
    """Generate a comprehensive analysis report"""
    report = []
    
    # Health Check Analysis
    report.append("=== HEALTH CHECK ANALYSIS ===")
    report.append(f"Total health checks: {len(health_df)}")
    report.append(f"Time period: {health_df['timestamp'].min()} to {health_df['timestamp'].max()}")
    report.append("")
    
    # Server availability
    uptime_stats = health_df.groupby('server_id')['status'].apply(
        lambda x: f"{(x == 'UP').sum()}/{len(x)} ({(x == 'UP').sum()/len(x)*100:.1f}%)"
    )
    report.append("Server Availability:")
    for server, stats in uptime_stats.items():
        report.append(f"  Server {server}: {stats}")
    report.append("")
    
    # Response time statistics
    healthy_df = health_df[health_df['status'] == 'UP']
    if not healthy_df.empty:
        response_stats = healthy_df.groupby('server_id')['response_time'].agg(['mean', 'std', 'min', 'max'])
        report.append("Response Time Statistics (ms) - Healthy Servers Only:")
        for server, stats in response_stats.iterrows():
            report.append(f"  Server {server}: Mean={stats['mean']:.2f}, Std={stats['std']:.2f}, "
                         f"Min={stats['min']:.2f}, Max={stats['max']:.2f}")
    report.append("")
    
    # Request Analysis
    report.append("=== REQUEST ANALYSIS ===")
    report.append(f"Total requests: {len(forward_df[forward_df['action'] == 'ARRIVE'])}")
    
    request_types = forward_df[forward_df['action'] == 'ARRIVE']['op_type'].value_counts()
    report.append("Request types:")
    for op_type, count in request_types.items():
        report.append(f"  {op_type}: {count} requests")
    report.append("")
    
    # Server load distribution
    forward_actions = forward_df[forward_df['action'] == 'FORWARD']
    server_load = forward_actions['server_id'].value_counts().sort_index()
    report.append("Server Load Distribution:")
    for server, count in server_load.items():
        report.append(f"  Server {server}: {count} requests")
    
    # Save report
    with open(f'{output_dir}/analysis_report.txt', 'w') as f:
        f.write('\n'.join(report))
    
    return report

def main():
    """Main analysis function"""
    print("Loading log files...")
    
    # Find all log files
    health_logs = glob.glob('health_log_*.csv')
    forward_logs = glob.glob('forward_log_*.csv')
    
    if not health_logs or not forward_logs:
        print("No log files found! Make sure to run the load balancer first.")
        return
    
    health_dfs = {}
    forward_dfs = {}
    
    # Parse all health logs
    for health_file in health_logs:
        algo_name = os.path.basename(health_file).replace('health_log_', '').replace('.csv', '')
        health_dfs[algo_name] = parse_health_log(health_file)
        print(f"Loaded health log: {health_file} ({len(health_dfs[algo_name])} records)")
    
    # Parse all forward logs
    for forward_file in forward_logs:
        algo_name = os.path.basename(forward_file).replace('forward_log_', '').replace('.csv', '')
        forward_dfs[algo_name] = parse_forward_log(forward_file)
        print(f"Loaded forward log: {forward_file} ({len(forward_dfs[algo_name])} records)")
    
    # Create plots for each algorithm
    for algo_name, health_df in health_dfs.items():
        print(f"\nCreating plots for {algo_name}...")
        algo_output_dir = f'plots/{algo_name}'
        
        if algo_name in forward_dfs:
            create_health_plots(health_df, algo_output_dir)
            create_forward_plots(forward_dfs[algo_name], algo_output_dir)
            generate_report(health_df, forward_dfs[algo_name], algo_output_dir)
    
    # Create comparison plots if multiple algorithms
    if len(health_dfs) > 1 and len(forward_dfs) > 1:
        print("\nCreating comparison plots...")
        create_comparison_plots(health_dfs, forward_dfs, 'plots/comparison')
    
    print(f"\nAnalysis complete! Check the 'plots' directory for generated graphs.")
    print("Generated plots:")
    for root, dirs, files in os.walk('plots'):
        for file in files:
            if file.endswith(('.png', '.txt')):
                print(f"  {os.path.join(root, file)}")

if __name__ == "__main__":
    main()