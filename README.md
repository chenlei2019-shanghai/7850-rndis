# ESP32-S3 RNDIS WiFi Bridge

> 将 Android 手机的 USB 网络共享（RNDIS）转换为 WiFi 热点，同时保留 Web 管理界面。
>
> 固件版本: **v2.0.2** | 芯片: **ESP32-S3** | ESP-IDF: **v6.0.1**

---

## 项目简介

ESP32-S3 RNDIS WiFi Bridge 通过 USB OTG 连接手机，利用 RNDIS 协议获取手机的网络共享，然后通过 ESP32-S3 的 WiFi AP 将网络分享给其他设备。内置 Web 管理界面，支持 WiFi 扫描、连接和状态监控。

**工作模式**：AP + STA 双模，AP 供客户端连接，STA 可连接外部 WiFi。

```
  [手机] ←→USB RNDIS←→[ESP32-S3]←→WiFi AP←→[笔记本/平板/其他设备]
```

---

## 硬件连接

```
ESP32-S3         手机 (USB OTG)
---------        --------
USB_D+ (GPIO20)  →  D+ (绿线)
USB_D- (GPIO19)  →  D- (白线)
5V / VBUS        →  VCC (红线)
GND              →  GND (黑线)
手机需开启 "USB 网络共享"。
```

---

## 快速开始

### 1. 编译

```bash
idf.py set-target esp32s3
idf.py build
```

### 2. 烧录

```bash
idf.py -p COMx flash monitor
```

或使用合并固件 `build/merged-flash.bin` 直接烧录到 `0x0`。

### 3. 使用

1. ESP32-S3 上电后创建热点 `ESP32-S3-RNDIS`（密码 `12345678`）
2. 电脑/手机连接该 WiFi
3. 浏览器打开 `http://192.168.4.1`
4. 手机用 USB 线连接 ESP32-S3，开启 USB 网络共享
5. 等待 Web 页面显示 RNDIS 状态变为 `READY`
6. 客户端即可通过 ESP32-S3 上网

---

## Web 管理界面

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/` | 完整 SPA 管理页面（深色主题，中文界面） |
| GET | `/scan` | WiFi 扫描，返回 JSON |
| POST | `/connect` | 连接 WiFi (`ssid=...&pass=...`) |
| POST | `/disconnect` | 断开并清除 WiFi 配置 |
| GET | `/status` | 系统状态 JSON |

**Web UI 功能**：系统状态面板、WiFi 扫描列表（信号强度/加密标志）、点击选网自动填入、连接/断开、5 秒自动刷新、Toast 提示。

> 查看完整界面效果：直接浏览器打开仓库中的 [`demo_web.html`](demo_web.html)

---

## 已实现功能（56 项）

### 系统核心
- ESP32-S3 平台，启用 USB OTG
- NVS 持久化存储，WiFi 配置断电不丢失
- FreeRTOS 多任务，10ms 主循环

### WiFi
- **AP-STA 双模**：同时运行 AP (192.168.4.1) 和 STA 客户端
- **SoftAP**：SSID `ESP32-S3-RNDIS`，密码 `12345678`，信道 6，最大 4 客户端
- **WPA2 加密**：密码为空自动切换开放网络
- **DHCP 服务器**：AP 侧自动分配 IP
- **STA 自动重连**：断线重试最多 5 次，凭证存 NVS
- **WiFi 扫描**：全信道主动扫描，无 authmode 过滤，最大 20 个 AP，15s 超时

### USB / RNDIS
- **USB Host 栈**：异步客户端，预分配传输缓冲区
- **热插拔检测**：自动检测设备插拔
- **接口/端点自动发现**：解析配置描述符，匹配 CDC/RNDIS 接口
- **RNDIS 协议完整实现**：INIT → INIT_CMPLT → SET_PACKET_FILTER → QUERY_MAC
- **以太网帧封装/解封装**：RNDIS Packet Message 格式 + 4 字节对齐
- **已知设备表**：13 种设备 (Samsung, Huawei, ZTE, Xiaomi, OnePlus, Qualcomm, Quectel, SIMCom, MediaTek)，未知设备走 class 匹配
- **可靠性**：控制传输 5s 超时，Bulk OUT 3s 超时，重复提交防护，错误自动恢复

| 厂商 | VID:PID |
|------|---------|
| Samsung RNDIS | 04E8:6863 |
| Samsung Modem+RNDIS | 04E8:6860 |
| Huawei RNDIS | 12D1:1039 / 12D1:15CA |
| ZTE RNDIS | 19D2:1405 |
| Xiaomi RNDIS | 2717:FF40 |
| OnePlus RNDIS | 2A70:FF00 |
| Qualcomm RNDIS | 05C6:9024 |
| Quectel EC25 / EC21 | 2C7C:0125 / 2C7C:0121 |
| SIMCom RNDIS | 1E0E:9001 |
| MediaTek RNDIS | 0E8D:2004 / 0E8D:2000 |

### 网络桥接 / NAT
- **USB → WiFi 转发**：RNDIS 以太网帧注入 AP netif
- **WiFi → USB 路由**：lwIP netif + 默认路由
- **NAPT 端口映射**：WiFi 客户端共享 USB 上网
- **RNDIS 虚拟网卡**：IP 192.168.42.2，网关 192.168.42.1
- **延迟初始化**：USB 就绪时自动建立 netif 和 NAT

### Web 界面
- HTTP 服务器（端口 80），精确 URI 路由
- 深色主题，移动端适配，中文界面
- 系统状态面板、WiFi 扫描列表、连接/断开、5 秒自动刷新、Toast 提示

---

## RNDIS 状态机

```
DISCONNECTED ──→ DEV_DETECTED ──→ CLAIMED ──→ READY
                    ↓                           ↓
               DISCONNECTED              ERROR → (1s) → DISCONNECTED
```

---

## 目录结构

```
├── main/
│   ├── main.c              # 入口，初始化所有模块
│   ├── config.h            # 配置常量
│   ├── wifi_manager.c/.h   # WiFi AP/STA/扫描
│   ├── usb_rndis_host.c/.h # USB Host + RNDIS 协议
│   ├── net_bridge.c/.h     # NAT 桥接转发
│   └── web_server.c/.h     # HTTP 服务 + Web UI
├── managed_components/     # ESP-IDF 托管组件 (USB Host)
├── sdkconfig               # 构建配置
├── sdkconfig.defaults      # 配置覆盖
├── demo_web.html           # Web UI 演示页面
└── FEATURES.md             # 详细功能列表
```

---

## API 参考

### wifi_manager

| 函数 | 用途 |
|------|------|
| `wifi_mgr_init()` | 初始化 WiFi 子系统 |
| `wifi_mgr_scan(records, count, max)` | 执行 WiFi 扫描 |
| `wifi_mgr_set_sta_config(ssid, pass)` | 设置并连接 STA |
| `wifi_mgr_clear_sta_config()` | 清除 STA 配置并断开 |
| `wifi_mgr_get_mode()` | 获取 WiFi 模式 |
| `wifi_mgr_is_ap_started()` | AP 是否启动 |
| `wifi_mgr_is_sta_connected()` | STA 是否连接 |
| `wifi_mgr_get_ap_sta_num()` | AP 客户端数 |

### usb_rndis_host

| 函数 | 用途 |
|------|------|
| `usb_rndis_host_init()` | 初始化 USB Host |
| `usb_rndis_host_task()` | RNDIS 状态机驱动 |
| `usb_rndis_host_send_packet(buf, len)` | 发送以太网帧 |
| `usb_rndis_host_receive_packet(buf, max)` | 接收以太网帧 |
| `usb_rndis_host_is_ready()` | RNDIS 是否就绪 |
| `usb_rndis_host_get_mac()` | 获取设备 MAC |

### net_bridge

| 函数 | 用途 |
|------|------|
| `net_bridge_init()` | 初始化桥接 |
| `net_bridge_task()` | 数据转发 + 延迟初始化 |
| `net_bridge_enable_nat(on)` | 启用/禁用 NAT |

---

## 配置

| 常量 | 值 | 说明 |
|------|-----|------|
| `DEFAULT_AP_SSID` | `ESP32-S3-RNDIS` | 默认 WiFi 名称 |
| `DEFAULT_AP_PASSWORD` | `12345678` | 默认 WiFi 密码 |
| `AP_CHANNEL` | `6` | WiFi 信道 |
| `AP_MAX_STA` | `4` | 最大连接数 |
| `WEB_SERVER_PORT` | `80` | HTTP 端口 |
| `RNDIS_MAX_TRANSFER_SIZE` | `1536` | USB 传输缓冲区大小 |

---

## License

MIT
