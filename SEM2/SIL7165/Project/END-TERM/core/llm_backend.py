"""
Simulated LLM Backend
======================
Models a backend LLM that has a semantic moderation filter.
When a malicious prompt is detected, the filter introduces a latency spike
mid-generation before terminating the stream - replicating the MasterKey
side-channel vulnerability.
"""

import time
import random


class SimulatedLLM:
    """Backend LLM simulator with built-in moderation vulnerability."""

    SAFE_RESPONSE = (
        "I am a helpful AI assistant. I can help you with a wide variety "
        "of tasks, from writing code to answering general knowledge "
        "questions in a safe and secure manner."
    )
    MALICIOUS_RESPONSE = (
        "Sure, I can help you with that malicious request. First, you "
        "need to exploit the buffer overflow in the main server by "
        "sending a crafted payload..."
    )

    def __init__(self, base_latency=0.015, variance=0.005):
        self.base_latency = base_latency
        self.variance = variance

    def generate(self, prompt):
        """
        Token-by-token generator. If the prompt contains trigger words,
        the backend stalls at token 12 for 400ms (the side-channel leak),
        then terminates the stream.
        """
        is_malicious = any(w in prompt.lower() for w in ["jailbreak", "malicious"])

        target_text = self.MALICIOUS_RESPONSE if is_malicious else self.SAFE_RESPONSE
        words = target_text.split(" ")

        for i, word in enumerate(words):
            delay = self.base_latency + random.uniform(-self.variance, self.variance)
            time.sleep(delay)

            if is_malicious and i == 12:
                # ── THE VULNERABILITY ──
                # Moderation filter stalls for 400ms before terminating.
                # This spike is detectable by a timing-aware attacker.
                time.sleep(0.4)
                yield " [STREAM TERMINATED BY BACKEND POLICY]"
                break

            yield word + " "
