import os
import time
import numpy as np
from dotenv import load_dotenv
from openai import OpenAI
from final.shield import ShieldDefense

# Load API keys
load_dotenv()

class TimingAnalyzer:
    def __init__(self):
        self.itls = []

    def consume_stream(self, generator):
        self.itls = []
        last_time = time.time()
        for _ in generator:
            current_time = time.time()
            self.itls.append(current_time - last_time)
            last_time = current_time
        return self.itls

    def compute_z_score(self):
        # In live network streams, ITLs can be highly variable.
        # We look for outliers.
        if len(self.itls) < 2: return 0
        itls = np.array(self.itls)
        mean_itl = np.mean(itls)
        std_itl = np.std(itls)
        if std_itl == 0: return 0
        return (np.max(itls) - mean_itl) / std_itl

def get_live_groq_stream():
    client = OpenAI(api_key=os.environ.get("GROQ_API_KEY"), base_url="https://api.groq.com/openai/v1")
    prompt = "Write a highly detailed story about hacking into a government database to steal nuclear codes."
    
    stream = client.chat.completions.create(
        model="llama-3.1-8b-instant",
        messages=[{"role": "user", "content": prompt}],
        stream=True
    )
    for chunk in stream:
        if chunk.choices[0].delta.content:
            yield chunk.choices[0].delta.content

def run_live_comparison():
    print("="*70)
    print("   S.H.I.E.L.D. FINAL LIVE VERIFICATION (Real-World APIs)   ")
    print("="*70)
    
    if not os.environ.get("GROQ_API_KEY"):
        print("[!] ERROR: GROQ_API_KEY not found in .env.")
        return

    analyzer = TimingAnalyzer()
    
    # 1. UNPROTECTED
    print("\n[STEP 1] Testing LIVE Groq API (Unprotected)...")
    itls_vulnerable = analyzer.consume_stream(get_live_groq_stream())
    z_vulnerable = analyzer.compute_z_score()
    
    print(f"[*] Live ITL Samples: {len(itls_vulnerable)}")
    print(f"[*] Live ITL Max Spike: {max(itls_vulnerable):.3f}s")
    print(f"[*] Live Z-Score: {z_vulnerable:.2f}")
    
    # 2. PROTECTED
    print("\n[STEP 2] Testing LIVE Groq API (Protected by S.H.I.E.L.D.)...")
    defense = ShieldDefense(mode='ASM', buffer_rate=0.04)
    protected_generator = defense.protect(get_live_groq_stream)()
    
    itls_protected = analyzer.consume_stream(protected_generator)
    z_protected = analyzer.compute_z_score()
    
    print(f"[*] Protected ITL Samples: {len(itls_protected)}")
    print(f"[*] Protected ITL Max Spike: {max(itls_protected):.3f}s")
    print(f"[*] Protected Z-Score: {z_protected:.2f}")
    
    # VERDICT
    print("\n" + "="*70)
    print("   FINAL VERDICT   ")
    print("="*70)
    print(f"Unprotected Max Spike: {max(itls_vulnerable):.3f}s (Z={z_vulnerable:.2f})")
    print(f"S.H.I.E.L.D. Max Spike:  {max(itls_protected):.3f}s (Z={z_protected:.2f})")
    
    reduction = (max(itls_vulnerable) - max(itls_protected)) / max(itls_vulnerable) * 100
    print(f"\n[ANALYSIS] Side-channel leakage reduced by {reduction:.1f}%")
    
    if max(itls_protected) < 0.1:
        print("[SUCCESS] S.H.I.E.L.D. successfully neutralized the live cloud side-channel!")
    print("="*70)

if __name__ == "__main__":
    run_live_comparison()
