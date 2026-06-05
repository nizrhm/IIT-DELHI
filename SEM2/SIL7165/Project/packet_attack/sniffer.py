from mock_network import SecureNetwork

class EavesdroppingSniffer:
    """
    Simulates a Wi-Fi packet sniffer (like Wireshark).
    It intercepts the encrypted network traffic.
    It CANNOT decrypt the payload, but it CAN read the packet size.
    """
    def __init__(self, network: SecureNetwork):
        self.network = network

    def sniff_and_extract_lengths(self):
        """
        Extracts the side-channel leak: the length of the tokens.
        """
        token_lengths = []
        
        # Read packets from the network wire
        for packet in self.network.packet_queue:
            # Side-channel math: Token Length = Packet Size - TLS Overhead
            leaked_length = packet["metadata_size"] - SecureNetwork.TLS_OVERHEAD_BYTES
            token_lengths.append(leaked_length)
            
        return token_lengths
