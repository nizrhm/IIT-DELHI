import struct
data = []
data.extend([0x01, 1])
for i in range(50000):
    data.extend([0x01, i]) 
    data.append(0x30)      
data.append(0x23)          
data.append(0xFF)          

with open("deep_test.bin", "wb") as f:
    for x in data:
        f.write(struct.pack('i', x))