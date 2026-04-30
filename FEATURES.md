# ESP32-S3 RNDIS WiFi Bridge — 已实现功能列表

> 固件版本: **v2.0.2** | 目标芯片: **ESP32-S3** | ESP-IDF: **v6.0.1**

---

## 一、系统核心

| # | 功能 | 说明 |
|---|------|------|
| 1 | ESP32-S3 平台支持 | 目标芯片 ESP32-S3，启用 USB OTG |
| 2 | NVS 持久化存储 | WiFi STA 配置保存到 Flash，断电不丢失 |
| 3 | FreeRTOS 多任务 | 主循环 10ms 周期驱动各模块任务 |

---

## 二、WiFi 模块 (`wifi_manager`)

### 2.1 模式与配置

| # | 功能 | 说明 |
|---|------|------|
| 4 | AP-STA 双模 | 同时运行 AP (192.168.4.1) 和 STA 客户端 |
| 5 | SoftAP 热点 | SSID: `ESP32-S3-RNDIS`, 密码: `12345678`, 信道 6, 最大 4 客户端 |
| 6 | WPA2 加密 | AP 默认 WPA2-PSK，密码为空时自动切为开放网络 |
| 7 | 静态 AP IP | AP 子网 192.168.4.0/24，网关 192.168.4.1 |
| 8 | DHCP 服务器 | AP 侧内置 DHCP，自动分配 IP 给连接设备 |
| 9 | AP 客户端计数 | 实时查询当前连接到 AP 的设备数量 |

### 2.2 STA 客户端

| # | 功能 | 说明 |
|---|------|------|
| 10 | STA 凭证持久化 | SSID/密码存 NVS，上电自动加载 |
| 11 | 自动重连 | STA 断线自动重连，最多重试 5 次 |
| 12 | 手动连接 | 通过 Web UI 输入 SSID/密码连接 |
| 13 | 手动断开 | 断开 STA 并清除已保存配置 |

### 2.3 WiFi 扫描

| # | 功能 | 说明 |
|---|------|------|
| 14 | 主动扫描 | 专用 FreeRTOS 任务 (8KB 栈)，全信道主动扫描 |
| 15 | 无阈值过滤 | 扫描结果不做 authmode 过滤，开放/WPA/WPA2 网络全部可见 |
| 16 | 最大 20 个 AP | 单次扫描最多返回 20 个 AP 记录 |
| 17 | 15 秒超时 | 扫描超时自动停止 |
| 18 | 扫描错误信息 | 可通过 API 获取上一次扫描的错误详情 |

---

## 三、USB / RNDIS 模块 (`usb_rndis_host`)

### 3.1 USB Host 基础

| # | 功能 | 说明 |
|---|------|------|
| 19 | USB Host 栈初始化 | 注册异步客户端，预分配控制/Bulk IN/Bulk OUT 传输缓冲区 |
| 20 | 设备热插拔检测 | 检测 USB 设备插拔事件（NEW_DEV / DEV_GONE） |
| 21 | 接口自动发现 | 解析配置描述符，自动匹配 CDC/RNDIS 接口 (class 0x02 / 0xEF) |
| 22 | 端点自动发现 | 自动识别 Bulk IN / Bulk OUT 端点地址 |
| 23 | 已知设备匹配 | 预置 13 种设备 VID/PID 表，未知设备走 class 匹配备选 |

### 3.2 RNDIS 协议

| # | 功能 | 说明 |
|---|------|------|
| 24 | RNDIS 初始化序列 | INIT → INIT_CMPLT → SET_PACKET_FILTER → QUERY_MAC |
| 25 | 数据包过滤器 | 设置 directed + broadcast + multicast 过滤 |
| 26 | MAC 地址查询 | 通过 OID_802_3_CURRENT_ADDRESS 读取设备 MAC |
| 27 | 以太网帧封装 | 发送方向：以太网帧 → RNDIS Packet Message → Bulk OUT |
| 28 | 以太网帧解封装 | 接收方向：Bulk IN → RNDIS Packet Message → 剥离出以太网帧 |
| 29 | 4 字节对齐填充 | RNDIS 消息自动对齐到 4 字节边界 |

### 3.3 状态机

| 状态 | 说明 |
|------|------|
| `DISCONNECTED` | 无设备连接 |
| `DEV_DETECTED` | 检测到新设备，待打开 |
| `CLAIMED` | 设备已打开，接口已声明 |
| `READY` | RNDIS 初始化完成，可收发数据 |
| `ERROR` | 初始化失败，等待 1 秒后复位重试 |

### 3.4 传输可靠性

| # | 功能 | 说明 |
|---|------|------|
| 30 | 超时处理 | 控制传输 5s 超时，Bulk OUT 3s 超时 |
| 31 | 重复提交防护 | `s_bulk_in_pending` 标志防止同时提交多个 Bulk IN |
| 32 | 错误恢复 | 失败后进入 ERROR 状态，1 秒后自动复位到 DISCONNECTED 重试 |

### 3.5 已知设备表 (13 种)

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

---

## 四、网络桥接 / NAT (`net_bridge`)

### 4.1 数据转发

| # | 功能 | 说明 |
|---|------|------|
| 33 | USB → WiFi 转发 | RNDIS 收到的以太网帧剥离 14B 头后注入 AP netif |
| 34 | WiFi → USB 转发 | 通过 lwIP netif + 默认路由将出站流量发往 USB |
| 35 | 以太网帧构造 | 手动组装目的 MAC + 源 MAC + EtherType + IP 载荷 |

### 4.2 NAT / 路由

| # | 功能 | 说明 |
|---|------|------|
| 36 | lwIP NAPT | AP 接口启用 NAPT 端口映射，WiFi 客户端共享 USB 上网 |
| 37 | RNDIS 虚拟网卡 | lwIP netif: IP 192.168.42.2, 网关 192.168.42.1 |
| 38 | IPv6 链路本地 | 自动生成 IPv6 link-local 地址 |
| 39 | 默认路由 | RNDIS 网卡设为系统默认路由 |
| 40 | 延迟初始化 | USB 设备就绪时自动建立 netif 和 NAT，无需手动触发 |

---

## 五、Web 界面 (`web_server`)

### 5.1 HTTP 服务

| # | 功能 | 说明 |
|---|------|------|
| 41 | HTTP 服务器 | 端口 80，栈 12KB，超时 20s |
| 42 | 精确 URI 匹配 | 默认精确匹配路由，5 个端点各司其职 |

### 5.2 API 端点

| 方法 | 路径 | 返回 | 功能 |
|------|------|------|------|
| GET | `/` | HTML | 完整 SPA 管理界面 |
| GET | `/scan` | JSON | WiFi 扫描 (`{"networks":[...],"error":"","debug":""}`) |
| POST | `/connect` | text | 连接 WiFi (`ssid=...&pass=...`) |
| POST | `/disconnect` | text | 断开并清除 WiFi 配置 |
| GET | `/status` | JSON | 系统状态 (模式/AP/STA/RNDIS/USB/MAC) |

### 5.3 Web UI 功能

| # | 功能 | 说明 |
|---|------|------|
| 43 | 深色主题 | 适配移动端的暗色 UI，卡片式布局 |
| 44 | 系统状态面板 | 2 列网格显示 WiFi 模式、AP 状态、STA 状态、RNDIS、USB 信息 |
| 45 | WiFi 扫描列表 | 点击扫描，显示 SSID / 信号图标 / 加密锁标志 / dBm 值 |
| 46 | 点击选网 | 点击扫描结果自动填入 SSID 并跳转到连接页 |
| 47 | SSID + 密码连接 | 输入密码后连接，开放网络自动提示无需密码 |
| 48 | 断开连接 | 一键断开并清除已保存的 WiFi 配置 |
| 49 | 自动刷新 | 每 5 秒自动轮询 `/status` 更新状态面板 |
| 50 | Toast 提示 | 浮动动画提示操作结果 |
| 51 | 标签页切换 | 扫描页 / 连接页 Tab 切换 |
| 52 | 加载动画 | 扫描时 CSS 旋转动画进度指示 |
| 53 | 中文界面 | 全中文标签和提示 |

### 5.4 响应式设计

| # | 功能 | 说明 |
|---|------|------|
| 54 | Viewport 适配 | `<meta viewport>` 移动端适配 |
| 55 | 触摸友好 | 按钮尺寸适配手指操作 |
| 56 | No-Cache 头 | 动态内容设置 `Cache-Control: no-cache` |

---

## 六、API 总览

### 6.1 wifi_manager (11 个公开接口)

| 函数 | 用途 |
|------|------|
| `wifi_mgr_init()` | 初始化 WiFi 子系统 |
| `wifi_mgr_task()` | 周期维护 (预留) |
| `wifi_mgr_get_mode()` | 获取 WiFi 模式 |
| `wifi_mgr_is_ap_started()` | AP 是否已启动 |
| `wifi_mgr_is_sta_connected()` | STA 是否已连接 |
| `wifi_mgr_get_ap_sta_num()` | AP 已连接客户端数 |
| `wifi_mgr_get_sta_ssid()` | 当前 STA SSID |
| `wifi_mgr_get_last_scan_err()` | 上次扫描错误信息 |
| `wifi_mgr_scan()` | 执行 WiFi 扫描 |
| `wifi_mgr_set_sta_config()` | 设置并连接 STA |
| `wifi_mgr_clear_sta_config()` | 清除 STA 配置并断开 |

### 6.2 usb_rndis_host (9 个公开接口)

| 函数 | 用途 |
|------|------|
| `usb_rndis_host_init()` | 初始化 USB Host |
| `usb_rndis_host_task()` | RNDIS 状态机驱动 |
| `usb_rndis_host_is_ready()` | RNDIS 是否就绪 |
| `usb_rndis_host_is_connected()` | USB 设备是否已连接 |
| `usb_rndis_host_state_str()` | 状态字符串 |
| `usb_rndis_host_send_packet()` | 发送以太网帧 |
| `usb_rndis_host_receive_packet()` | 接收以太网帧 |
| `usb_rndis_host_get_mac()` | 获取设备 MAC |
| `usb_rndis_host_get_vid/pid()` | 获取 USB VID/PID |

### 6.3 net_bridge (3 个公开接口)

| 函数 | 用途 |
|------|------|
| `net_bridge_init()` | 初始化桥接 |
| `net_bridge_task()` | 数据转发 + 延迟初始化 |
| `net_bridge_enable_nat()` | 启用/禁用 NAT |

### 6.4 web_server (1 个公开接口 + 5 端点)

| 接口 | 说明 |
|------|------|
| `web_server_start()` | 启动 HTTP 服务 |
| `GET /` | SPA 管理页面 |
| `GET /scan` | WiFi 扫描 API |
| `POST /connect` | 连接 WiFi |
| `POST /disconnect` | 断开 WiFi |
| `GET /status` | 系统状态 API |
