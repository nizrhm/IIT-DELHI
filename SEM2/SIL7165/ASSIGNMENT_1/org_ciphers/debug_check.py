import os
import difflib

# Configuration
ANSWERS_DIR = "./answers"
OUTPUTS_DIR = "./outputs"
NUM_CASES = 6

def normalize_text(text):
    """
    Normalizes text to match the bash script's behavior:
    1. Removes trailing whitespace from every line.
    2. Unifies line endings to avoid Windows/Linux mismatch (\r\n vs \n).
    """
    lines = text.splitlines()
    # rstrip() removes trailing whitespace like the 'sed' command in the bash script
    normalized_lines = [line.rstrip() for line in lines]
    # Remove empty lines at the end of the file (optional, but often helpful)
    while normalized_lines and not normalized_lines[-1]:
        normalized_lines.pop()
    return normalized_lines

def compare_files(case_num):
    ans_path = os.path.join(ANSWERS_DIR, f"answer-{case_num}.txt")
    out_path = os.path.join(OUTPUTS_DIR, f"output-{case_num}.txt")

    # 1. Check if files exist
    if not os.path.exists(ans_path):
        print(f"[Case {case_num}] SKIPPED: Answer file not found ({ans_path})")
        return False
    if not os.path.exists(out_path):
        print(f"[Case {case_num}] FAIL: Output file not found ({out_path})")
        return False

    # 2. Read and Normalize
    with open(ans_path, 'r', encoding='utf-8', errors='ignore') as f:
        ans_lines = normalize_text(f.read())
    
    with open(out_path, 'r', encoding='utf-8', errors='ignore') as f:
        out_lines = normalize_text(f.read())

    # 3. Compare
    if ans_lines == out_lines:
        print(f"[Case {case_num}] PASS")
        return True
    else:
        print(f"[Case {case_num}] FAIL")
        print(f"--- Differences in Case {case_num} ---")
        
        # Print the first difference found
        diff = difflib.unified_diff(
            ans_lines, 
            out_lines, 
            fromfile=f'answer-{case_num}', 
            tofile=f'output-{case_num}', 
            lineterm=''
        )
        
        # Show first few lines of difference
        for line in list(diff)[:10]: 
            print(line)
        print("-------------------------------")
        return False

if __name__ == "__main__":
    print(f"Checking {NUM_CASES} cases...\n")
    passed = 0
    total = 0
    
    for i in range(1, NUM_CASES + 1):
        if compare_files(i):
            passed += 1
        total += 1
        
    print(f"\nSummary: {passed}/{total} Passed")