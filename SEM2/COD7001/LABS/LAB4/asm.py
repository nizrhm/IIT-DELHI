import struct
import sys

OPCODES = {
    "PUSH": 0x01, "POP": 0x02, "DUP": 0x03,
    "ADD": 0x10, "SUB": 0x11, "MUL": 0x12, "DIV": 0x13, "CMP": 0x14,
    "JMP": 0x20, "JZ": 0x21, "JNZ": 0x22,
    "STORE": 0x30, "LOAD": 0x31,
    "CALL": 0x40, "RET": 0x41,
    "HALT": 0xFF
}

def load_lines(filename):
    """Recursively loads lines and handles INCLUDE directives."""
    lines = []
    try:
        with open(filename, 'r') as f:
            for line in f:
                clean_line = line.split(';')[0].strip()
                if not clean_line:
                    continue
                if clean_line.startswith("INCLUDE"):
                    parts = clean_line.split()
                    if len(parts) > 1:
                        # Recursively load the library file
                        lines.extend(load_lines(parts[1]))
                else:
                    lines.append(clean_line)
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found.")
        sys.exit(1)
    return lines

def assemble(input_file, output_file):
    labels = {}
    instructions = []
    
    # Merge all files (main + libraries) into one list of lines
    raw_lines = load_lines(input_file)
    
    # --- Pass 1: Label Resolution ---
    current_idx = 0
    for line in raw_lines:
        if line.endswith(':'):
            label_name = line[:-1]
            if label_name in labels:
                print(f"Error: Duplicate label '{label_name}'")
                sys.exit(1)
            labels[label_name] = current_idx
        else:
            parts = line.split()
            current_idx += 1  # Opcode
            if len(parts) > 1:
                current_idx += 1  # Operand
    
    # --- Pass 2: Bytecode Generation ---
    binary_data = []
    for line in raw_lines:
        if line.endswith(':'):
            continue  # Skip label declarations
            
        parts = line.split()
        mnemonic = parts[0].upper()
        
        if mnemonic not in OPCODES:
            print(f"Error: Unknown instruction '{mnemonic}'")
            sys.exit(1)
        
        binary_data.append(OPCODES[mnemonic])
        
        if len(parts) > 1:
            arg = parts[1]
            if arg in labels:
                binary_data.append(labels[arg])
            else:
                try:
                    binary_data.append(int(arg))
                except ValueError:
                    print(f"Error: Invalid operand '{arg}'")
                    sys.exit(1)

    # Write to binary file
    with open(output_file, 'wb') as f:
        for value in binary_data:
            f.write(struct.pack('i', value))
    
    print(f"Successfully assembled {input_file} to {output_file} (including libraries)")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 asm.py <input.asm> <output.bin>")
    else:
        assemble(sys.argv[1], sys.argv[2])