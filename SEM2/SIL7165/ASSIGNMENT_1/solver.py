import sys
import random
import math

# The alphabets defined in the assignment
PLAIN_CHARS = "abcdefghijklmnopqrstuvwxyz"
CIPHER_CHARS = "1234567890@#$zyxwvutsrqpon"

class HillClimbingSolver:
    def __init__(self, ciphertext, quadgram_file="english_quadgrams.txt"):
        self.ciphertext = ciphertext
        # Filter only valid cipher characters for analysis
        self.clean_cipher = [c for c in ciphertext if c in CIPHER_CHARS]
        self.quadgrams = self.load_quadgrams(quadgram_file)
        self.n = len(self.clean_cipher)

    def load_quadgrams(self, filename):
        # Load English quadgram statistics (log probabilities)
        quads = {}
        total = 0
        try:
            with open(filename, 'r') as f:
                for line in f:
                    key, count = line.split()
                    quads[key.lower()] = int(count)
                    total += int(count)
            # Convert counts to log probabilities to avoid underflow
            for key in quads:
                quads[key] = math.log10(float(quads[key]) / total)
            self.floor = math.log10(0.01 / total) # Probability for non-existent quadgrams
        except FileNotFoundError:
            # Fallback if file missing (Logic will still run but effectively random without stats)
            sys.stderr.write("Warning: english_quadgrams.txt not found.\n")
            return {}
        return quads

    def get_score(self, text):
        # Calculate log probability of the text based on quadgrams
        score = 0
        for i in range(len(text) - 3):
            sub = text[i:i+4]
            if sub in self.quadgrams:
                score += self.quadgrams[sub]
            else:
                score += self.floor
        return score

    def decipher(self, key_map):
        # Apply the mapping to the clean cipher text for scoring
        # key_map is a dictionary {cipher_char: plain_char}
        return "".join([key_map[c] for c in self.clean_cipher])

    def solve(self):
        # Initial random key
        # Create a list of plaintext chars to shuffle
        key_chars = list(PLAIN_CHARS)
        random.shuffle(key_chars)
        
        # Map CIPHER_CHARS -> Shuffled PLAIN_CHARS
        current_map = {c: p for c, p in zip(CIPHER_CHARS, key_chars)}
        
        # Initial score
        current_text = self.decipher(current_map)
        current_score = self.get_score(current_text)
        
        # Optimization loop (Hill Climbing)
        # We run multiple iterations to avoid local maxima
        max_score = -float('inf')
        best_map = current_map.copy()
        
        # Timeout safety or iteration limit could go here, 
        # but for this assignment, a fixed high number of iterations usually works.
        # Given the 2 min timeout, we can run aggressively.
        
        for _ in range(20): # Restarts (avoid local maxima)
            random.shuffle(key_chars)
            current_map = {c: p for c, p in zip(CIPHER_CHARS, key_chars)}
            current_text = self.decipher(current_map)
            current_score = self.get_score(current_text)
            
            count = 0
            while count < 2500: # Stability check
                # Swap two characters in the key
                a = random.randint(0, 25)
                b = random.randint(0, 25)
                
                # Get the cipher chars at these indices
                c1, c2 = CIPHER_CHARS[a], CIPHER_CHARS[b]
                
                # Swap the plaintext mapping
                current_map[c1], current_map[c2] = current_map[c2], current_map[c1]
                
                text = self.decipher(current_map)
                score = self.get_score(text)
                
                if score > current_score:
                    current_score = score
                    count = 0 # Reset stability count if we found improvement
                else:
                    # Revert swap
                    current_map[c1], current_map[c2] = current_map[c2], current_map[c1]
                    count += 1
            
            if current_score > max_score:
                max_score = current_score
                best_map = current_map.copy()

        return best_map

    def format_output(self, best_map):
        # 1. Generate full plaintext
        full_plaintext = []
        cipher_chars_set = set(self.ciphertext)
        
        for char in self.ciphertext:
            if char in CIPHER_CHARS:
                full_plaintext.append(best_map[char])
            else:
                full_plaintext.append(char) # Keep punctuation/spaces
        
        deciphered_text = "".join(full_plaintext)

        # 2. Generate Key String
        # The key string is the sequence of 26 ciphertext characters 
        # corresponding to a, b, c ... z
        
        # First, invert the map: {plain: cipher}
        reverse_map = {v: k for k, v in best_map.items()}
        
        key_string = []
        for p_char in PLAIN_CHARS:
            # Check if this plaintext char was actually used/mapped.
            # However, the assignment says: "If a character does not appear in cipher text 
            # then you should substitute x"
            
            # Find the cipher char that maps to this p_char
            c_char = reverse_map[p_char]
            
            # Check if c_char actually appeared in the input text
            if c_char in cipher_chars_set:
                key_string.append(c_char)
            else:
                key_string.append('x')

        print(f"Deciphered Plaintext: {deciphered_text}")
        print(f"Deciphered Key: {''.join(key_string)}")

if __name__ == "__main__":
    # Read from stdin as per checkscript
    input_text = sys.stdin.read().strip()
    
    solver = HillClimbingSolver(input_text)
    best_mapping = solver.solve()
    solver.format_output(best_mapping)