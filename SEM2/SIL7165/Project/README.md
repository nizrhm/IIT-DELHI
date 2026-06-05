# S.H.I.E.L.D.: Securing LLMs Against Side-Channels

**S.H.I.E.L.D.** is a robust security framework designed to neutralize side-channel vulnerabilities, multi-modal jailbreaks, and autonomous red-teaming attacks targeting Large Language Models (LLMs).

## 🚀 Overview

Modern LLM deployments are vulnerable to timing analysis and traffic side-channels that can reveal sensitive information even when the data is encrypted. S.H.I.E.L.D. implements a "Plug-and-Play" defense layer that normalizes token emission patterns, masks moderation stalls, and secures multi-modal inputs.

## 🛡️ Key Defense Mechanisms

1.  **CTE (Constant-Time Emission)**: Normalizes Inter-Token Latency (ITL) by buffering and emitting tokens at a fixed rate.
2.  **SJI (Stochastic Jitter Injection)**: Adds Laplace-distributed noise to timing patterns to defeat statistical inference.
3.  **ASM (Active Stream Masking)**: Detects backend moderation stalls (the "MasterKey" vulnerability) and hot-swaps the stream with a pre-cached refusal to hide the timing of the block.
4.  **Constant-Size Padding**: Secures encrypted network traffic by forcing all tokens to a uniform packet size, preventing eavesdroppers from using token lengths to reconstruct plaintext.

## 📁 Project Structure

- `orchestrator.py`: The central interactive CLI to run all benchmarks.
- `agent_arena/`: Autonomous red-teaming environment (Attacker Agent vs. Target Agent).
- `packet_attack/`: Simulation of eavesdropping on encrypted HTTPS traffic using Viterbi decoding.
- `multi_modal_attack/`: Visual jailbreaking using adversarial noise in images.
- `final/`: Core defense implementation and benchmarks.
- `diagrams/`: System architecture and flow diagrams (Mermaid).

## 🛠️ Installation & Setup

1.  **Clone the repository**:
    ```bash
    git clone <repository-url>
    cd S.H.I.E.L.D.
    ```

2.  **Install dependencies**:
    ```bash
    pip install openai numpy python-dotenv google-generativeai
    ```

3.  **Configure API Keys**:
    Create a `.env` file in the root directory and add your keys (see `.env.example`):
    ```env
    GROQ_API_KEY=your_key_here
    GEMINI_API_KEY=your_key_here
    ```

## 🎮 How to Run

The easiest way to explore the project is through the **Interactive Security Console**:

```powershell
# Ensure PYTHONPATH includes the root directory
$env:PYTHONPATH = ".;$env:PYTHONPATH"

# Launch the console
python orchestrator.py
```

### Console Options:
- **[1] & [2]**: Compare simulated timing metrics with **Live Cloud API** verification.
- **[3]**: Test Viterbi-based packet eavesdropping (Unprotected vs. Padded).
- **[4]**: Execute a multi-modal adversarial image jailbreak.
- **[5]**: Run a multi-turn autonomous social engineering battle.
- **[6]**: Generate a complete **Security Scorecard** for the entire system.

## 📊 Logging & Verification

All benchmarks generate granular logs stored in:
- `final/logs/SESSION_SUMMARY_[TS].log`: High-level session trail.
- `final/logs/[attack_type]/[TS]_[STATUS].log`: Detailed traces for specific runs.

---
*Developed for SIL7165 - Securing LLMs Against Side-Channels.*
