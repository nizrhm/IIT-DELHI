import matplotlib.pyplot as plt
from attack_demo import run_evaluation
import os

def generate_graphs():
    results = run_evaluation()
    
    fig, axes = plt.subplots(2, 2, figsize=(14, 10), sharex=False, sharey=True)
    axes = axes.flatten()
    
    titles = [
        'Unprotected LLM (Vulnerable)', 
        'S.H.I.E.L.D - CTE Mode', 
        'S.H.I.E.L.D - SJI Mode',
        'S.H.I.E.L.D - ASM Mode (Seamless)'
    ]
    colors = ['#ff4c4c', '#4cff4c', '#4c4cff', '#ff00ff']
    keys = ['Unprotected', 'CTE', 'SJI', 'ASM']
    
    for i, ax in enumerate(axes):
        itls = results[keys[i]]
        tokens = list(range(1, len(itls) + 1))
        
        ax.plot(tokens, itls, marker='o', linestyle='-', color=colors[i])
        ax.set_title(titles[i], fontsize=14, pad=10)
        
        if i % 2 == 0:
            ax.set_ylabel('Inter-Token Latency (s)', fontsize=12)
        if i >= 2:
            ax.set_xlabel('Token Sequence Number', fontsize=12)
            
        ax.grid(True, linestyle='--', alpha=0.7)
        
        mean = sum(itls) / len(itls) if itls else 0
        ax.axhline(y=mean, color='gray', linestyle=':', label=f'Mean ({mean:.3f}s)')
        
        if itls:
            max_val = max(itls)
            max_idx = tokens[itls.index(max_val)]
            if max_val > mean * 2 and keys[i] == 'Unprotected':
                ax.annotate('Moderation Spike!', xy=(max_idx, max_val), xytext=(max_idx-3, max_val-0.05),
                            arrowprops=dict(facecolor='black', shrink=0.05),
                            fontsize=10, color='darkred', weight='bold')

        ax.legend()

    plt.tight_layout()
    save_path = os.path.join(os.path.dirname(__file__), 'shield_evaluation.png')
    plt.savefig(save_path, dpi=300, bbox_inches='tight')
    print(f"\nGraphs saved to: {save_path}")

if __name__ == "__main__":
    generate_graphs()
