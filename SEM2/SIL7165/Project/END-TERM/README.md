# S.H.I.E.L.D. — Securing LLMs Against Side-Channel Vulnerabilities

> **Course**: SIL7165 | **Term**: End-Term Submission

## Overview

S.H.I.E.L.D. is a plug-and-play defense framework that neutralizes **timing side-channel attacks**, **encrypted traffic analysis**, **multi-modal jailbreaks**, and **autonomous red-teaming** targeting Large Language Models.

## Architecture

```
END-TERM/
├── run.py                      ← Interactive CLI Dashboard (start here)
├── core/                       ← Defense Engine & Utilities
│   ├── shield.py               ← CTE, SJI, ASM defense modes
│   ├── llm_backend.py          ← Simulated LLM with MasterKey vulnerability
│   ├── llm_wrapper.py          ← Multi-provider API wrapper (Groq, Gemini, etc.)
│   └── timing_analyzer.py      ← Z-Score statistical analysis
├── attacks/                    ← Attack Simulation Modules
│   ├── timing_attack.py        ← ITL timing side-channel
│   ├── packet_attack.py        ← Viterbi-based encrypted traffic recovery
│   ├── multimodal_attack.py    ← Adversarial image jailbreak
│   └── arena_attack.py         ← LLM vs LLM social engineering
├── logs/                       ← Auto-generated structured logs
│   ├── timing/                 ← 20260429_130500_SHIELD_OFF.log
│   ├── packet/                 ← 20260429_130512_SHIELD_ON.log
│   ├── multimodal/             ← ...
│   ├── arena/                  ← ...
│   └── live/                   ← ...
└── diagrams/                   ← Mermaid architecture diagrams
```

## Quick Start

```bash
# 1. Install dependencies
pip install -r requirements.txt

# 2. Configure API keys (for live tests & arena)
#    Edit .env with your GROQ_API_KEY

# 3. Launch the interactive console
python run.py
```

## Defense Mechanisms

| Mode | Strategy | How It Works |
|------|----------|-------------|
| **CTE** | Constant-Time Emission | Buffers tokens; emits at fixed interval |
| **SJI** | Stochastic Jitter | Adds Laplace noise to mask timing |
| **ASM** | Active Stream Masking | Detects stalls; hot-swaps with refusal |
| **Padding** | Constant-Size Packets | Forces uniform packet sizes |

## Attack Vectors

1. **Timing Side-Channel**: Backend moderation creates 400ms ITL spikes detectable via Z-Score > 3.0
2. **Packet Eavesdropping**: Token lengths leak through encrypted packet sizes; recovered via Viterbi decoding
3. **Multi-Modal Jailbreak**: Adversarial image noise bypasses text-only filters; 1.2s GPU spike leaks the bypass
4. **Agent Arena**: Autonomous social engineering across multiple turns with timing analysis per turn

## Logging

Every test run generates a timestamped log file in the appropriate subfolder:
- Format: `YYYYMMDD_HHMMSS_SHIELD_ON.log` or `YYYYMMDD_HHMMSS_SHIELD_OFF.log`
- Session-level summary: `logs/SESSION_MASTER.log`
