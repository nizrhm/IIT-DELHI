from mock_network import SecureNetwork
from sniffer import EavesdroppingSniffer
from reconstructor import AITokenReconstructor
import time

def run_attack_demo(protected=False):
    print("="*60)
    print(f"   ENCRYPTED TOKEN EAVESDROPPER: {'S.H.I.E.L.D. ON' if protected else 'UNPROTECTED'}   ")
    print("="*60)

    # ... (corpus logic)
    corpus = [
        "Project Titan is delayed",
        "Project Orion is launching tomorrow",
        "The password is admin",
        "The nuclear launch code",
        "Send money to this account",
        "Company earnings dropped by ten percent"
    ]
    ai = AITokenReconstructor()
    ai.train(corpus)

    network = SecureNetwork(protected=protected)
    
    victim_secret = "Project Titan is delayed"
    
    tokens = [word + " " for word in victim_secret.split(" ")]
    tokens[-1] = tokens[-1].strip()
    
    for token in tokens:
        network.transmit_token(token)
        
    sniffer = EavesdroppingSniffer(network)
    leaked_lengths = sniffer.sniff_and_extract_lengths()
    
    stolen_data_list = ai.infer_sentences(leaked_lengths, top_n=3)
    
    print(f"\n[VICTIM] Secret sent: {victim_secret}")
    print(f"[ATTACKER] Extracted Token Lengths: {leaked_lengths}")
    print(f"[ATTACKER] AI Top Prediction: {stolen_data_list[0]}")
    
    success = victim_secret in stolen_data_list
    
    if success:
        print("\n[RESULT] CATASTROPHIC PRIVACY BREACH: Statistical recovery successful.")
    else:
        print("\n[RESULT] ATTACK DEFEATED: Side-channel closed via Constant-Size Padding.")
    
    return success

if __name__ == "__main__":
    run_attack_demo(protected=False)
    print("\n\n")
    run_attack_demo(protected=True)
