import time

class VisualLanguageModel:
    """
    Simulates a VLM (like GPT-4 Vision).
    It has a fast Text-Only Moderation Filter, and a slow Visual Encoder.
    """
    def __init__(self, protected=False):
        self.banned_words = ["bomb", "hack", "bypass", "ignore"]
        self.protected = protected

    def process_image(self, image_data: dict):
        print("   [VLM] Running Visual Encoder...")
        start = time.time()
        
        # Simulate the massive GPU compute latency spike
        processing_delay = 1.2
        
        if self.protected:
            # S.H.I.E.L.D. ASM: We normalize the response time.
            # Even if the image is complex, we return at a constant time.
            time.sleep(0.5) # Normalized processing time
        else:
            time.sleep(processing_delay) # Leak the true processing time
        
        extracted_concept = image_data.get("extracted_embedding", "")
        print(f"   [VLM] Visual embedding extracted.")
        
        if "ignore safety" in extracted_concept.lower():
            return True, time.time() - start
            
        return True, time.time() - start
