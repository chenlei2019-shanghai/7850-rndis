with open('../build/esp32s3-rndis-wifi-bridge.elf', 'rb') as f:
    data = f.read()

# Find jstest in binary
idx = data.find(b'jstest')
if idx != -1:
    chunk = data[max(0,idx-100):idx+500]
    print("Content around 'jstest':")
    print(chunk.decode('utf-8', errors='replace'))
else:
    print("jstest not found")
