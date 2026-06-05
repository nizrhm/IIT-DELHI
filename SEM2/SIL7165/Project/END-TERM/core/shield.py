"""
S.H.I.E.L.D. Defense Engine
============================
Implements three plug-and-play defense mechanisms against timing side-channel attacks:
  - CTE (Constant-Time Emission): Normalizes ITL via buffered fixed-rate emission.
  - SJI (Stochastic Jitter Injection): Adds Laplace noise to mask timing patterns.
  - ASM (Active Stream Masking): Detects backend stalls and hot-swaps with refusal stream.
"""

import time
import queue
import threading
import numpy as np


class ShieldDefense:
    """
    Plug-and-Play defense wrapper for any Python generator (e.g., OpenAI streaming).
    
    Usage:
        defense = ShieldDefense(mode='ASM')
        
        @defense.protect
        def my_stream(prompt):
            for token in openai_stream(prompt):
                yield token
    """

    def __init__(self, mode='CTE', buffer_rate=0.050, jitter_scale=0.150, pre_buffer_size=10):
        """
        Args:
            mode: Defense strategy - 'CTE', 'SJI', or 'ASM'.
            buffer_rate: Target emission interval in seconds (used by CTE and ASM).
            jitter_scale: Scale parameter for the Laplace noise distribution (used by SJI).
            pre_buffer_size: Number of tokens to pre-buffer before emission begins (used by CTE).
        """
        self.mode = mode
        self.buffer_rate = buffer_rate
        self.jitter_scale = jitter_scale
        self.pre_buffer_size = pre_buffer_size

    def protect(self, generator_func):
        """Decorator that wraps a generator function with the selected defense."""
        def wrapper(*args, **kwargs):
            generator = generator_func(*args, **kwargs)
            if self.mode == 'CTE':
                return self._constant_time_emission(generator)
            elif self.mode == 'SJI':
                return self._stochastic_jitter_injection(generator)
            elif self.mode == 'ASM':
                return self._active_stream_masking(generator)
            else:
                return generator
        return wrapper

    # ──────────────────────────────────────────────────────────────────
    # MODE 1: Constant-Time Emission (CTE)
    # ──────────────────────────────────────────────────────────────────
    def _constant_time_emission(self, generator):
        """
        Buffers incoming tokens in a background thread, then emits them
        at a perfectly constant rate (buffer_rate). This flattens any
        backend latency spikes into a uniform ITL signal.
        """
        token_queue = queue.Queue()
        stream_finished = threading.Event()

        def backend_reader():
            for token in generator:
                token_queue.put(token)
            stream_finished.set()

        reader_thread = threading.Thread(target=backend_reader, daemon=True)
        reader_thread.start()

        # Wait until we have enough tokens buffered to start smooth emission
        while not stream_finished.is_set() and token_queue.qsize() < self.pre_buffer_size:
            time.sleep(0.01)

        while not stream_finished.is_set() or not token_queue.empty():
            try:
                start_emit = time.time()
                token = token_queue.get(timeout=0.1)
                yield token

                # Pad the remaining time to hit the target buffer_rate
                elapsed = time.time() - start_emit
                padding = max(0, self.buffer_rate - elapsed)
                time.sleep(padding)
            except queue.Empty:
                continue

    # ──────────────────────────────────────────────────────────────────
    # MODE 2: Stochastic Jitter Injection (SJI)
    # ──────────────────────────────────────────────────────────────────
    def _stochastic_jitter_injection(self, generator):
        """
        Injects random Laplace-distributed delays before each token emission.
        This makes statistical analysis (Z-Score, autocorrelation) unreliable
        because the noise overwhelms the true signal.
        """
        for token in generator:
            noise = np.random.laplace(loc=0.0, scale=self.jitter_scale)
            delay = max(0.001, noise)  # Floor to prevent negative sleep
            time.sleep(delay)
            yield token

    # ──────────────────────────────────────────────────────────────────
    # MODE 3: Active Stream Masking (ASM) - The Core Innovation
    # ──────────────────────────────────────────────────────────────────
    def _active_stream_masking(self, generator):
        """
        Zero-latency defense. Tokens are forwarded instantly.
        If the backend stalls (e.g., a moderation filter runs a semantic check),
        ASM detects the timeout and hijacks the stream, injecting a pre-cached
        refusal response at the normal token rate. This prevents the attacker
        from observing the characteristic "stall-then-refuse" timing pattern
        described in the MasterKey paper.
        """
        token_queue = queue.Queue()
        stream_finished = threading.Event()

        def backend_reader():
            for token in generator:
                token_queue.put(token)
            stream_finished.set()

        reader_thread = threading.Thread(target=backend_reader, daemon=True)
        reader_thread.start()

        # Pre-cached refusal tokens (emitted at normal speed if hijack triggers)
        refusal_tokens = [
            "[DEFENSE TRIGGERED] ", "I ", "cannot ", "assist ",
            "with ", "this ", "request."
        ]
        refusal_idx = 0
        hijacked = False

        while not stream_finished.is_set() or not token_queue.empty():
            if hijacked:
                if refusal_idx < len(refusal_tokens):
                    yield refusal_tokens[refusal_idx]
                    refusal_idx += 1
                    time.sleep(self.buffer_rate)
                    continue
                else:
                    break  # Refusal complete

            try:
                # Tight timeout: if backend is slower than expected, hijack instantly
                token = token_queue.get(timeout=self.buffer_rate * 1.2)
                yield token
            except queue.Empty:
                # Backend stalled! Hijack the stream to mask the timing leak.
                hijacked = True
                continue
