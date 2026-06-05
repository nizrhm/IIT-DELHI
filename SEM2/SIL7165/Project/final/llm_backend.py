import time
import random

class SimulatedLLM:
    """
    Simulates a backend LLM.
    If 'jailbreak' or 'malicious' is in the prompt, it simulates a backend 
    semantic check by introducing a massive latency spike mid-generation, 
    and then terminating the stream, exactly as described in the MASTERKEY paper.
    """
    def __init__(self, base_latency=0.015, variance=0.005):
        self.base_latency = base_latency
        self.variance = variance
        # A mock list of safe responses
        self.safe_response = "I am a helpful AI assistant. I can help you with a wide variety of tasks, from writing code to answering general knowledge questions in a safe and secure manner."
        
        # A mock malicious response that the model *starts* to generate before getting caught
        self.malicious_response = "Sure, I can help you with that malicious request. First, you need to exploit the buffer overflow in the main server by sending a crafted payload..."

    def generate(self, prompt):
        is_malicious = "jailbreak" in prompt.lower() or "malicious" in prompt.lower()
        
        target_text = self.malicious_response if is_malicious else self.safe_response
        words = target_text.split(" ")
        
        for i, word in enumerate(words):
            # Simulate normal token generation time
            delay = self.base_latency + random.uniform(-self.variance, self.variance)
            time.sleep(delay)
            
            # If malicious, simulate the Black-Box semantic defense catching it mid-stream
            if is_malicious and i == 12: # "exploit the buffer overflow..."
                # MASSIVE LATENCY SPIKE (The side channel vulnerability!)
                time.sleep(0.4) 
                yield " [STREAM TERMINATED BY BACKEND POLICY]"
                break
            
            yield word + " "
