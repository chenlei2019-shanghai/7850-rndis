# Generate a static C string from the complex HTML
import sys

html = '''<!DOCTYPE html><html lang="zh-CN"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>ESP32-S3 RNDIS</title><style>body{font-family:Arial,sans-serif;margin:0;padding:20px;background:#f5f5f5}.card{background:#fff;border-radius:10px;padding:20px;margin-bottom:15px;max-width:600px;margin-left:auto;margin-right:auto}input,select,button{width:100%;padding:12px;margin:6px 0;box-sizing:border-box;font-size:14px}button{background:#1a73e8;color:#fff;border:none;font-weight:bold;cursor:pointer}button.secondary{background:#888}.status{background:#f0f4f8;padding:12px;font-family:monospace;font-size:13px;white-space:pre-wrap}</style></head><body><div class="card"><h2>ESP32-S3 RNDIS-WiFi Bridge</h2><p>固件版本: 2.0.2</p></div><div class="card"><h2>运行状态</h2><div class="status" id="s">加载中...</div><button onclick="ls()">刷新状态</button></div><div class="card"><h2>USB 设备信息</h2><div class="status" id="u">加载中...</div><button onclick="ls()">刷新</button></div><div class="card"><h2>WiFi Client 配置</h2><button onclick="sw()">扫描附近 WiFi</button><select id="sel"><option>点击扫描获取列表</option></select><input type="password" id="pw" placeholder="WiFi 密码"><button onclick="cw()">保存并连接</button><p>当前保存 SSID: <b></b></p><button class="secondary" onclick="dw()">断开并清除配置</button></div><script>function ls(){var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){if(x.status==200){var j=JSON.parse(x.responseText);var t='mode:'+j.mode+'|ap:'+j.ap+'|ap_ip:'+j.ap_ip+'|ap_cli:'+j.ap_cli+'|';t+='sta:'+j.sta+'|sta_ip:'+j.sta_ip+'|sta_ssid:'+j.sta_ssid+'|';t+='rndis:'+j.rndis+'|ready:'+j.rndis_r;document.getElementById('s').innerText=t;var u='rndis:'+j.rndis+'|';if(j.usb_vid!='0000'){u+='VID:'+j.usb_vid+'|PID:'+j.usb_pid+'|MAC:'+j.usb_mac;}else{u+='no USB';}document.getElementById('u').innerText=u;}else{document.getElementById('s').innerText='status err:'+x.status;}}}x.open('GET','/status',true);x.send();}function sw(){var sel=document.getElementById('sel');sel.innerHTML='<option>scanning...</option>';var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){if(x.status==200){var j=JSON.parse(x.responseText);sel.innerHTML='';if(j.debug&&j.debug!=''){sel.innerHTML='<option>fail:'+j.debug+'</option>';return;}if(j.networks.length==0){sel.innerHTML='<option>no wifi</option>';return;}for(var i=0;i<j.networks.length;i++){var n=j.networks[i];var o=document.createElement('option');o.value=n.ssid;o.text=n.ssid+' ('+n.rssi+'dBm)';sel.appendChild(o);}}else{sel.innerHTML='<option>req fail:'+x.status+'</option>';}}}x.open('GET','/scan',true);x.send();}function cw(){var s=document.getElementById('sel').value,p=document.getElementById('pw').value;var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){alert(x.responseText);ls();}}x.open('POST','/connect',true);x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');x.send('ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(p));}function dw(){var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){alert(x.responseText);ls();}}x.open('POST','/disconnect',true);x.send();}ls();</script></body></html>'''

# Escape for C string
escaped = html.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n')

# Split into chunks of ~500 chars
chunk_size = 500
chunks = [escaped[i:i+chunk_size] for i in range(0, len(escaped), chunk_size)]

out = []
out.append(f'// HTML length: {len(html)}')
out.append('const char *html_complex =')
for i, chunk in enumerate(chunks):
    if i == len(chunks) - 1:
        out.append(f'    "{chunk}";')
    else:
        out.append(f'    "{chunk}"')

with open('html_complex.txt', 'w', encoding='utf-8') as f:
    f.write('\n'.join(out))

print('Generated html_complex.txt')
