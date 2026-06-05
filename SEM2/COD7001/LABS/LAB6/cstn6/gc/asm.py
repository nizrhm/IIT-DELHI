import struct
import sys

OPCODES = {
    "PUSH":  0x01,  
    "POP":   0x02,    
    "DUP":   0x03,
    "ADD":   0x10, 
    "SUB":   0x11, 
    "MUL":   0x12, 
    "DIV":   0x13, 
    "CMP":   0x14,
    "JMP":   0x20, 
    "JZ":    0x21, 
    "JNZ":   0x22,
    "GC":    0x23,    
    "PAIR":  0x30, 
    "LOAD":  0x31, 
    "STORE": 0x32, 
    "CALL":  0x40, 
    "RET":   0x41,
    "HALT":  0xFF
}

def assemble(input_file, output_file):
    labels = {}
    instructions = []
    current_idx = 0
    
    try:
        with open(input_file, 'r') as f:
            for line_num, line in enumerate(f, 1):
                clean_line = line.split(';')[0].strip()
                if not clean_line:
                    continue
                
                if clean_line.endswith(':'):
                    label_name = clean_line[:-1]
                    if label_name in labels:
                        print(f"Error line {line_num}: Duplicate label '{label_name}'")
                        sys.exit(1)
                    labels[label_name] = current_idx
                else:
                    instructions.append((line_num, clean_line))
                    parts = clean_line.split()
                    current_idx += 1  
                    if len(parts) > 1:
                        current_idx += 1  

        binary_data = []
        for line_num, line in instructions:
            parts = line.split()
            mnemonic = parts[0].upper()
            
            if mnemonic not in OPCODES:
                print(f"Error line {line_num}: Unknown instruction '{mnemonic}'")
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
                        print(f"Error line {line_num}: Invalid operand '{arg}'")
                        sys.exit(1)

        with open(output_file, 'wb') as f:
            for value in binary_data:
                f.write(struct.pack('i', value))
        
        print(f"Successfully assembled {input_file} to {output_file}")

    except FileNotFoundError:
        print(f"Error: File '{input_file}' not found.")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 asm.py <input.asm> <output.bin>")
    else:
        assemble(sys.argv[1], sys.argv[2])