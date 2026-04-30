with open('web_server.c', 'rb') as f:
    content = f.read()
start = content.find(b'<script>')
end = content.find(b'</script>', start)
js_section = content[start:end]
print('JS section length:', len(js_section))
newline_count = js_section.count(b'\n')
backslash_n_count = js_section.count(b'\\n')
print(f'Real newlines (0x0A): {newline_count}')
print(f'Backslash-n (0x5C 0x6E): {backslash_n_count}')

# Show first few real newlines
idx = 0
for _ in range(min(5, newline_count)):
    idx = js_section.find(b'\n', idx)
    if idx == -1:
        break
    ctx_start = max(0, idx - 30)
    ctx_end = min(len(js_section), idx + 30)
    ctx = js_section[ctx_start:ctx_end]
    print(f'\nReal newline at offset {idx}:')
    print(repr(ctx))
    idx += 1
