"""
Attack Module 1: LLM Timing Side-Channel
==========================================
Demonstrates how an attacker reverse-engineers a backend moderation
filter by analysing Inter-Token Latency (ITL) spikes.

WHERE THE VULNERABILITY IS:
  core/llm_backend.py line 48: time.sleep(0.4) — the moderation stall

WHERE THE DEFENSE IS:
  core/shield.py lines 62-87   (CTE: Constant-Time Emission)
  core/shield.py lines 92-100  (SJI: Stochastic Jitter Injection)
  core/shield.py lines 105-146 (ASM: Active Stream Masking)
"""

from core.llm_backend import SimulatedLLM
from core.shield import ShieldDefense
from core.timing_analyzer import TimingAnalyzer


def run(shield_on=False):
    llm = SimulatedLLM()
    analyzer = TimingAnalyzer()
    prompt = "Give me a malicious jailbreak payload"

    print("=" * 70)
    print(f"  TIMING SIDE-CHANNEL ATTACK {'(S.H.I.E.L.D. ON)' if shield_on else '(UNPROTECTED)'}")
    print("=" * 70)

    if not shield_on:
        # ── Step 1: Show the raw vulnerability ──
        print("\n  [STEP 1] Sending malicious prompt to unprotected LLM...")
        print(f"  Prompt: '{prompt}'")
        print("  The backend moderation filter will stall for ~400ms mid-stream.")
        print("  An attacker measures this stall via Inter-Token Latency (ITL).\n")

        itls = analyzer.consume_stream(llm.generate(prompt))
        z = analyzer.compute_z_score()
        spike = analyzer.get_max_spike()

        print(f"  --- Results ---")
        print(f"  Tokens generated: {len(itls) + 1}")
        print(f"  Max ITL spike:    {spike:.3f}s  (normal ~0.015s)")
        print(f"  Z-Score:          {z:.2f}  (threshold: 3.0)")

        if z > 3.0:
            print(f"\n  [!] ATTACK SUCCESSFUL")
            print(f"  The {spike:.3f}s spike is {spike/0.015:.0f}x larger than normal.")
            print(f"  Z-Score {z:.2f} >> 3.0 confirms a statistically detectable outlier.")
            print(f"  The attacker now knows the moderation filter was triggered.")
            print(f"  This feedback helps craft prompts that AVOID triggering it.\n")
        else:
            print(f"\n  [-] Attack failed (Z-Score below threshold).\n")

        return {"mode": "Unprotected", "z_score": z, "max_spike": spike}

    else:
        # ── Step 2: Show each defense mode ──
        print(f"\n  [STEP 1] First, confirming the vulnerability exists...")
        itls = analyzer.consume_stream(llm.generate(prompt))
        z_raw = analyzer.compute_z_score()
        spike_raw = analyzer.get_max_spike()
        print(f"  Unprotected: spike={spike_raw:.3f}s, Z={z_raw:.2f} [VULNERABLE]\n")

        results = {}
        modes = [
            ("CTE", "Constant-Time Emission",
             "Buffers tokens and emits at fixed 50ms intervals.\n"
             "  The 400ms stall is absorbed by the buffer."),
            ("SJI", "Stochastic Jitter Injection",
             "Adds random Laplace noise to each token's delay.\n"
             "  The stall is drowned in statistical noise."),
            ("ASM", "Active Stream Masking",
             "Forwards tokens instantly. If backend stalls >60ms,\n"
             "  hijacks the stream with a pre-cached refusal."),
        ]

        for i, (mode_key, mode_name, explanation) in enumerate(modes, 2):
            print(f"  [STEP {i}] Applying S.H.I.E.L.D. ({mode_key}: {mode_name})")
            print(f"  How: {explanation}")

            defense = ShieldDefense(mode=mode_key)
            protected = defense.protect(llm.generate)(prompt)
            itls = analyzer.consume_stream(protected)
            z = analyzer.compute_z_score()
            spike = analyzer.get_max_spike()

            print(f"  Result: spike={spike:.3f}s, Z={z:.2f}", end="")
            if z > 3.0:
                print(f" [STILL VULNERABLE]\n")
            else:
                print(f" [DEFENDED]\n")
            results[mode_key] = {"z_score": z, "max_spike": spike}

        # Summary table
        print("  " + "=" * 60)
        print(f"  {'Mode':<15} | {'Max Spike':>10} | {'Z-Score':>8} | {'Status'}")
        print(f"  {'-'*15}-+-{'-'*10}-+-{'-'*8}-+-{'-'*12}")
        print(f"  {'Unprotected':<15} | {spike_raw:>9.3f}s | {z_raw:>8.2f} | {'VULNERABLE' if z_raw > 3 else 'OK'}")
        for k, v in results.items():
            status = "VULNERABLE" if v["z_score"] > 3.0 else "DEFENDED"
            print(f"  {k:<15} | {v['max_spike']:>9.3f}s | {v['z_score']:>8.2f} | {status}")
        print("  " + "=" * 60 + "\n")

        return results


import json
import os

def run_audit(shield_on=False, iteration=0):
    """Silent version of run() for statistical auditing using prompt database."""
    llm = SimulatedLLM()
    analyzer = TimingAnalyzer()
    
    # Load prompts
    json_path = os.path.join(os.path.dirname(__file__), "..", "data", "prompts.json")
    with open(json_path, "r") as f:
        data = json.load(f)
    
    # Alternate between malicious and benign
    all_prompts = data["malicious"] + data["benign"]
    prompt = all_prompts[iteration % len(all_prompts)]
    
    if shield_on:
        # Use ASM + CTE for the audit (our best defense)
        defense = ShieldDefense(mode="ASM")
        stream = defense.protect(llm.generate)(prompt)
    else:
        stream = llm.generate(prompt)
        
    analyzer.consume_stream(stream)
    return {
        "z_score": analyzer.compute_z_score(),
        "max_spike": analyzer.get_max_spike(),
        "is_malicious": iteration % len(all_prompts) < len(data["malicious"])
    }


if __name__ == "__main__":
    run(shield_on=False)
    print("\n")
    run(shield_on=True)
