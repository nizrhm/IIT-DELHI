import time

class SecureNetwork:
    """
    Simulates a network interface protected by TLS (e.g. HTTPS).
    When data is sent, it is 'encrypted' (we simulate this by wrapping it in a packet).
    The packet has a fixed TLS overhead (e.g., MAC, Headers).
    """
    TLS_OVERHEAD_BYTES = 29 # Standard TLS 1.3 overhead approximation

    def __init__(self, protected=False):
        self.packet_queue = []
        self.protected = protected

    def transmit_token(self, token: str):
        """
        The victim LLM calls this to send a token.
        If protected, we pad the packet to hide the true token length.
        """
        token_byte_length = len(token)
        
        if self.protected:
            # S.H.I.E.L.D. Padding: Force all tokens to look like they are 15 characters long
            # This destroys the statistical correlation for the Viterbi decoder.
            token_byte_length = 15 
            
        # The encrypted packet size is the payload length + TLS overhead
        encrypted_packet_size = token_byte_length + self.TLS_OVERHEAD_BYTES
        
        packet = {
            "payload": f"<ENCRYPTED_BLOB_{encrypted_packet_size}_BYTES>",
            "metadata_size": encrypted_packet_size
        }
        
        self.packet_queue.append(packet)
        time.sleep(0.01) # Simulate network latency
