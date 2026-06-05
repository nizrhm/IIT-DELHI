import random
import os

# --- Configuration ---
PLAIN_CHARS = "abcdefghijklmnopqrstuvwxyz"
CIPHER_POOL = "1234567890@#$zyxwvutsrqpon"
INPUT_DIR = "./inputs"
ANSWER_DIR = "./answers"

# Ensure directories exist
os.makedirs(INPUT_DIR, exist_ok=True)
os.makedirs(ANSWER_DIR, exist_ok=True)

# Sample texts of similar length to the assignment (approx 300-500 chars)
sample_texts = [
    # Case 4: Technology
    "cryptography is the practice and study of techniques for secure communication in the presence of adversarial behavior. more generally, cryptography is about constructing and analyzing protocols that prevent third parties or the public from reading private messages. modern cryptography exists at the intersection of the disciplines of mathematics, computer science, information security, electrical engineering, digital signal processing, physics, and others.",
    
    # Case 5: Nature
    "photosynthesis is a process used by plants and other organisms to convert light energy into chemical energy that, through cellular respiration, can later be released to fuel the organism's activities. some of this chemical energy is stored in carbohydrate molecules, such as sugars and starches, which are synthesized from carbon dioxide and water. hence the name photosynthesis, from the greek phos, meaning light, and synthesis, meaning putting together.",
    
    # Case 6: History
    "the industrial revolution was the transition to new manufacturing processes in great britain, continental europe, and the united states, in the period from about seventeen sixty to sometime between eighteen twenty and eighteen forty. this transition included going from hand production methods to machines, new chemical manufacturing and iron production processes, the increasing use of steam power and water power, the development of machine tools and the rise of the mechanized factory system."
]

def generate_case(index, text):
    # 1. Create a Random Key Map
    # Shuffle the cipher characters
    shuffled_cipher = list(CIPHER_POOL)
    random.shuffle(shuffled_cipher)
    
    # Create map: Plain -> Cipher
    encrypt_map = {p: c for p, c in zip(PLAIN_CHARS, shuffled_cipher)}
    
    # 2. Encrypt the text
    ciphertext_chars = []
    used_cipher_chars = set()
    
    for char in text:
        lower_char = char.lower()
        if lower_char in PLAIN_CHARS:
            c = encrypt_map[lower_char]
            ciphertext_chars.append(c)
            used_cipher_chars.add(c)
        else:
            # Pass punctuation/spaces through unchanged
            ciphertext_chars.append(char)
            
    ciphertext = "".join(ciphertext_chars)

    # 3. Generate the Answer Key String
    # Format: Sequence of cipher chars corresponding to a-z. 
    # If a cipher char wasn't used in the text, put 'x'.
    key_string_list = []
    for p in PLAIN_CHARS:
        c = encrypt_map[p]
        if c in used_cipher_chars:
            key_string_list.append(c)
        else:
            key_string_list.append('x')
    
    key_string = "".join(key_string_list)

    # 4. Write Files
    # Write Input (Ciphertext)
    with open(f"{INPUT_DIR}/input-{index}.txt", "w", encoding="utf-8") as f:
        f.write(ciphertext)
        
    # Write Answer (Plaintext + Key)
    # We use explicit \n to ensure unix-style line endings if possible, 
    # but the format string matches the assignment exactly.
    with open(f"{ANSWER_DIR}/answer-{index}.txt", "w", encoding="utf-8") as f:
        f.write(f"Deciphered Plaintext: {text}\n")
        f.write(f"Deciphered Key: {key_string}\n") # Ensure newline at end matches typical unix files

    print(f"Generated Case {index}")

# --- Main Execution ---
if __name__ == "__main__":
    print("Generating test cases 4, 5, and 6...")
    for i, text in enumerate(sample_texts):
        case_num = i + 4
        generate_case(case_num, text)
    print("Done! You can now run ./checkscript.sh")