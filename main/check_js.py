import re

html_template = '''<!DOCTYPE html><html lang="zh-CN"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>ESP32-S3 RNDIS</title><style>body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5}.card{background:#fff;border-radius:10px;padding:20px;margin-bottom:15px;max-width:600px;margin-left:auto;margin-right:auto}input,select,button{width:100%;padding:12px;margin:6px 0;box-sizing:border-box;font-size:14px}button{background:#1a73e8;color:#fff;border:none;font-weight:bold;cursor:pointer}button.secondary{background:#888}.status{background:#f0f4f8;padding:12px;font-family:monospace;font-size:13px;white-space:pre-wrap}</style></head><body><div class="card"><h2>ESP32-S3 RNDIS-WiFi Bridge</h2><p>固件版本: 2.0.2</p></div><div class="card"><h2>运行状态</h2><div class="status" id="s">加载中...</div><button onclick="ls()">刷新状态</button></div><div class="card"><h2>USB 设备信息</h2><div class="status" id="u">加载中...</div><button onclick="ls()">刷新</button></div><div class="card"><h2>WiFi Client 配置</h2><button onclick="sw()">扫描附近 WiFi</button><select id="sel"><option>点击扫描获取列表</option></select><input type="password" id="pw" placeholder="WiFi 密码"><button onclick="cw()">保存并连接</button><p>当前保存 SSID: <b></b></p><button class="secondary" onclick="dw()">断开并清除配置</button></div><script>function ls(){var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){if(x.status==200){var j=JSON.parse(x.responseText);var t='WiFi模式: '+j.mode+'\nAP状态: '+j.ap+' (IP:'+j.ap_ip+')\nAP客户端: '+j.ap_cli+'\n';t+='STA状态: '+j.sta+' (IP:'+j.sta_ip+')\nSTA SSID: '+j.sta_ssid+'\n';t+='RNDIS: '+j.rndis+' 就绪:'+j.rndis_r;document.getElementById('s').innerText=t;var u='状态: '+j.rndis+'\n';if(j.usb_vid!='0000'){u+='VID: '+j.usb_vid+'\nPID: '+j.usb_pid+'\nMAC: '+j.usb_mac;}else{u+='未连接USB设备';}document.getElementById('u').innerText=u;}else{document.getElementById('s').innerText='状态获取失败';}}}x.open('GET','/status',true);x.send();}function sw(){var sel=document.getElementById('sel');sel.innerHTML='<option>扫描中...</option>';var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){if(x.status==200){var j=JSON.parse(x.responseText);sel.innerHTML='';if(j.debug&&j.debug!=''){sel.innerHTML='<option>扫描失败: '+j.debug+'</option>';return;}if(j.networks.length==0){sel.innerHTML='<option>未找到WiFi</option>';return;}for(var i=0;i<j.networks.length;i++){var n=j.networks[i];var o=document.createElement('option');o.value=n.ssid;o.text=n.ssid+' ('+n.rssi+'dBm)'+(n.secure?' [锁]':'');sel.appendChild(o);}}else{sel.innerHTML='<option>扫描请求失败: HTTP '+x.status+'</option>';}}}x.open('GET','/scan',true);x.send();}function cw(){var s=document.getElementById('sel').value,p=document.getElementById('pw').value;var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){alert(x.responseText);ls();}}x.open('POST','/connect',true);x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');x.send('ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p));}function dw(){var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){alert(x.responseText);ls();}}x.open('POST','/disconnect',true);x.send();}ls();</script></body></html>'''

m = re.search(r'<script>(.*?)</script>', html_template, re.DOTALL)
if m:
    js = m.group(1)
    print('JS length:', len(js))
    print('First 100 chars:', repr(js[:100]))
    print('Last 100 chars:', repr(js[-100:]))
    
    # Check for actual newlines inside JS
    lines = js.split('\n')
    if len(lines) > 1:
        print(f'ERROR: Found {len(lines)} real newlines in JS!')
        for i, line in enumerate(lines[:10]):
            print(f'  Line {i+1}: {line[:60]}')
    else:
        print('OK: No real newlines in JS')
    
    # Try to parse as JS-like structure
    print('\nChecking for unescaped quotes in strings...')
    in_string = False
    string_char = None
    for i, c in enumerate(js):
        if not in_string:
            if c in "'\"":
                in_string = True
                string_char = c
        else:
            if c == '\\':
                pass  # escaped char
            elif c == string_char:
                in_string = False
                string_char = None
            elif c == '\n':
                print(f'ERROR: Unescaped newline in string at position {i}')
                break
    else:
        if not in_string:
            print('OK: All strings properly closed')
        else:
            print(f'ERROR: Unclosed string (started with {string_char})')
