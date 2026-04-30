import re

# Read the compiled ELF to find the HTML string
with open('../build/esp32s3-rndis-wifi-bridge.elf', 'rb') as f:
    data = f.read()

# Search for "<script>" in the binary
idx = data.find(b'<script>')
if idx == -1:
    print("<script> not found in binary")
else:
    # Extract a chunk around <script>
    chunk = data[idx:idx+500]
    print("Binary content around <script>:")
    print(repr(chunk))
    print()
    
    # Check for real newlines (0x0A) inside the script section
    script_end = data.find(b'</script>', idx)
    if script_end != -1:
        script_data = data[idx:script_end]
        real_newlines = script_data.count(b'\n')
        backslash_n = script_data.count(b'\\n')
        print(f"Script section length: {len(script_data)}")
        print(f"Real newlines (0x0A) in script: {real_newlines}")
        print(f"Backslash-n (0x5C 0x6E) in script: {backslash_n}")
        
        if real_newlines > 0:
            print("\nFirst real newline context:")
            nl_idx = script_data.find(b'\n')
            ctx = script_data[max(0,nl_idx-30):nl_idx+30]
            print(repr(ctx))
