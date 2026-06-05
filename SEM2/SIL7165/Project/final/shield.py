import time
import queue
import threading
import random
import numpy as np

class ShieldDefense:
    def __init__(self, mode='CTE', buffer_rate=0.050, jitter_scale=0.150, pre_buffer_size=10):
        """
        mode: 'CTE' (Constant-Time Emission), 'SJI' (Stochastic Jitter), or 'ASM' (Active Stream Masking)
        """
        self.mode = mode
        self.buffer_rate = buffer_rate
        self.jitter_scale = jitter_scale
        self.pre_buffer_size = pre_buffer_size

    def protect(self, generator_func):
        """
        Plug-and-Play Wrapper:
        Takes ANY python generator (like an OpenAI streaming response) 
        and wraps it in the defense mechanism.
        """
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

    def _constant_time_emission(self, generator):
        token_queue = queue.Queue()
        stream_finished = threading.Event()

        def backend_reader():
            for token in generator:
                token_queue.put(token)
            stream_finished.set()

        reader_thread = threading.Thread(target=backend_reader)
        reader_thread.daemon = True
        reader_thread.start()

        while not stream_finished.is_set() and token_queue.qsize() < self.pre_buffer_size:
            time.sleep(0.01)

        while not stream_finished.is_set() or not token_queue.empty():
            try:
                start_emit = time.time()
                token = token_queue.get(timeout=0.1)
                yield token
                
                elapsed = time.time() - start_emit
                padding = max(0, self.buffer_rate - elapsed)
                time.sleep(padding)
            except queue.Empty:
                continue

    def _stochastic_jitter_injection(self, generator):
        for token in generator:
            noise = np.random.laplace(loc=0.0, scale=self.jitter_scale)
            delay = max(0.001, noise)
            time.sleep(delay)
            yield token

    def _active_stream_masking(self, generator):
        """
        Mode 3: The Holy Grail (Active Stream Masking).
        No pre-buffering. Instantly yields tokens.
        If the backend stalls for a moderation check, ASM hijacks the stream
        and seamlessly injects a refusal response without breaking the ITL timing.
        """
        token_queue = queue.Queue()
        stream_finished = threading.Event()

        def backend_reader():
            for token in generator:
                token_queue.put(token)
            stream_finished.set()

        reader_thread = threading.Thread(target=backend_reader)
        reader_thread.daemon = True
        reader_thread.start()

        refusal_tokens = ["[DEFENSE TRIGGERED] ", "I ", "cannot ", "assist ", "with ", "this ", "request."]
        refusal_idx = 0
        hijacked = False

        while not stream_finished.is_set() or not token_queue.empty():
            start_emit = time.time()
            
            if hijacked:
                # We have hijacked the stream. Pump out refusal tokens at normal speed.
                if refusal_idx < len(refusal_tokens):
                    yield refusal_tokens[refusal_idx]
                    refusal_idx += 1
                    time.sleep(self.buffer_rate)
                    continue
                else:
                    break

            try:
                # Tight timeout: if backend is slower than normal buffer rate, instantly hijack.
                token = token_queue.get(timeout=self.buffer_rate * 1.2)
                yield token
            except queue.Empty:
                # Timeout! The backend stalled. We assume it's running a semantic moderation check.
                # Hijack the stream to prevent a timing side-channel leak.
                hijacked = True
                continue
