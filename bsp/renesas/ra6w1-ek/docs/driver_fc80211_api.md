# RA6W1 `driver_fc80211.h` API 说明

本文说明：

`bsp/renesas/ra6w1-ek/wifi/supplicant/sdk/interfaces/wifi/stack/supplicant/src/drivers/driver_fc80211.h`

该头文件是 Renesas RA6WX/RA6W1 Wi-Fi SDK 中 fc80211 驱动层的接口汇总。它不是一个单独的“固件命令头文件”，而是把以下几层接口放在同一个文件中：

```text
RTOS/应用
   |
   +-- Renesas WIFI_* / rm_wifi_* 服务接口
          |
          +-- wpa_driver_fc80211_* 包装层
                 |
                 +-- rsdev_cfg80211_*（Renesas cfg80211 适配）
                 |
                 +-- fc80211_*（固件/UMAC 命令入口）
                        |
                        +-- rwnx/mac/firmware
```

## 1. 重要结论

### 1.1 `wpa_driver_fc80211_ops` 的性质

`wpa_driver_fc80211_ops` 定义在对应的 `.c` 文件中，不在本头文件中。它是 `wpa_supplicant`/`hostapd` 使用的 `struct wpa_driver_ops` 回调表。表中的每个成员把 supplicant 的通用操作映射到本头文件声明的 `driver_fc80211_*` 或 `wpa_driver_fc80211_*` 函数。

例如：

```c
.scan2      = driver_fc80211_scan2,
.associate  = wpa_driver_fc80211_associate,
.deinit     = driver_fc80211_deinit,
.get_capa   = wpa_driver_fc80211_get_capa,
```

它不是 RT-Thread `struct rt_wlan_dev_ops`，不能直接注册给 RT-Thread WLAN。适配层必须自己维护状态并转换数据结构。当前仓库中的独立示例见 [`wifi_fc80211_demo.c`](/home/rain/renesas/rt-thread/bsp/renesas/ra6w1-ek/wifi/app/wifi_fc80211_demo.c)：它不创建 `wpa_supplicant` 对象，也不调用 `WIFI_*`/`rm_wifi_*` 服务，而是直接使用 `romac4rtos_*`、MAC/driver task、`rwnx_cfg80211_*` 和底层 `fc80211_*` ABI。

### 1.2 不使用 supplicant 上下文时的限制

以下函数的参数中出现 `struct i802_bss *`、`struct wpa_driver_fc80211_data *` 或 `void *ctx`。这些对象不只是普通参数：

* `i802_bss` 保存接口索引、`wdev_id`、所属 driver 和事件上下文。
* `wpa_driver_fc80211_data` 保存扫描状态、连接状态、BSSID、SSID、能力和事件过滤状态。
* `ctx` 最终会被 `process_drv_event()` 用来调用 `wpa_supplicant_event()`。

因此，不能简单地调用 `wpa_driver_fc80211_drv_init(NULL, ...)` 后把返回值当作一个无上下文的裸句柄使用；异步事件到达时可能访问空的 supplicant 上下文。若明确不使用 supplicant 上下文，应走完整的底层生命周期：平台准备、`romac4rtos_initialize()`/`romac4rtos_config()`、MAC/driver task、`rwnx_cfg80211_add_iface()`，然后通过 `rsdev_cfg80211_scan()` 做参数转换并从 `TO_SUPP_QUEUE` 接收扫描完成事件。这里有一个容易混淆的 ABI 细节：头文件中的 `rwnx_cfg80211_scan()` 声明接收公开的 `struct cfg80211_scan_request *`，但当前随 SDK 提供的预编译 `librwnx_drv.a` 在该调用链中实际读取的是 `rsdev_cfg80211_scan()` 生成的私有扫描参数布局；不能把公开结构直接传给这个库函数。示例对 `fc80211_get_scan()` 需要的私有结果结构做了 ABI 对齐的本地副本，并逐项释放返回的结果；这些结构属于预编译库契约，升级库时必须重新检查布局。

### 1.3 返回值约定

除特殊说明外，本文件中的 `int` 接口遵循 Linux/hostapd 风格：

* `0`：成功；
* 负值：失败，通常是 `-errno`；
* 正值：某些查询接口会返回数值、长度、索引或能力位，不能一概当成错误。

头文件来自 FreeRTOS/裸机移植环境，具体工程可能把 `errno`、`EBUSY`、`ENOTSUPP` 等映射为 SDK 自己的错误码。调用者应以当前 SDK 的实现和链接库为准。

### 1.4 `rwnx` 头文件与 `driver_fc80211.h` 的分层关系

这两个头文件有包含关系，但职责不同：

| 文件/层 | 主要内容 | 面向的调用者 | 是否包含 supplicant 状态 |
|---|---|---|---|
| `rwnx_cfg_common.h` | 公共配置数据类型；当前版本主要是 `struct cfg80211_crypto_settings`，描述 WPA 版本、cipher、AKM、PSK/SAE、control port 等 | `rwnx_cfg_if.h`、`rwnx_cfg.h` 和 cfg80211 实现 | 否；它本身没有函数入口 |
| `rwnx_cfg_if.h` | cfg80211 风格的数据结构，例如 `cfg80211_scan_request`、`cfg80211_connect_params`、BSS、站点和 AP 参数 | rwnx cfg80211 实现、Renesas 适配层 | 否，但结构字段风格来自 Linux cfg80211 |
| `rwnx_cfg.h` | 低层操作入口，例如 `rwnx_cfg80211_add_iface()`、`rwnx_cfg80211_scan()`、connect/disconnect、key、AP、channel 操作 | 主机侧 driver/适配代码 | 设计上不需要完整 supplicant 上下文 |
| `fc80211_copy.h` | `FC80211_CMD_*` 命令、属性、枚举、`nlattr` 和消息编码/解析定义；相当于 Renesas 私有 nl80211 协议词汇表 | `fc80211_*` 和 `rsdev_cfg80211_*` 实现 | 否 |
| `driver_fc80211.h` | 同时声明 `fc80211_*` 固件/UMAC 操作、`rsdev_cfg80211_*` 适配操作、事件处理函数，以及 `wpa_driver_fc80211_*` 包装函数和私有结构 | wpa_supplicant/hostapd 的 driver backend | 后半部分是，需要 `global -> driver -> bss -> ctx` |

可以把它们理解为“数据模型”和“驱动适配契约”的关系，而不是两个互相替代的 API：

```text
RT-Thread WLAN / 应用
        |
        | 由我们编写的 rt_wlan_dev_ops 适配和状态管理
        v
rsdev_cfg80211_*       Renesas host 参数转换/资源管理
        |
        | 生成 cfg80211/私有扫描对象
        v
rwnx_cfg80211_*        rwnx cfg80211 操作入口
        |
        | 使用 FC80211_CMD_* 消息
        v
fc80211_*               UMAC/MAC/固件命令与异步事件
        |
        v
libmacsw.a + 固件/ROMAC
```

扫描时的关键数据流是：应用参数 -> host scan 参数 -> 私有 cfg80211 扫描对象 -> 固件扫描请求 -> `TO_SUPP_QUEUE` 完成事件 -> UMAC BSS 缓存 -> 应用自己的 `struct rt_wlan_info` 数组。每一步的结构体属于不同 ABI，不能用强制类型转换代替字段转换。

当前 SDK 还存在一个容易踩到的特殊情况：`rwnx_cfg.h` 的公开声明是
`rwnx_cfg80211_scan(u8, u8, struct cfg80211_scan_request *)`，但随工程提供的
预编译 `librwnx_drv.a` 在 `rsdev_cfg80211_scan()` 调用链中实际读取的是私有 host
扫描布局。因而本工程的无 supplicant demo 调用 `rsdev_cfg80211_scan()`，并在
[`wifi_fc80211_demo.c`](/home/rain/renesas/rt-thread/bsp/renesas/ra6w1-ek/wifi/app/wifi_fc80211_demo.c)
中显式提供 ABI 对齐的本地结构；不要把公开 `cfg80211_scan_request` 直接传入该预编译库。

该私有扫描对象的重要字段（Cortex-M 32 位 ABI 下）如下，供排查库升级或重新编译后的
结构变化：

| 偏移 | 字段 | 用途 |
|---:|---|---|
| `+0` | `wdev` | 当前无线设备，用于完成事件和结果缓存关联 |
| `+4` | 打包的 SSID 数组 | 每个 SSID 占 33 字节（32 字节内容 + 1 字节长度） |
| `+8` | `num_ssids` | SSID 数量，固件路径最多取 2 个 |
| `+12` | `num_channels` | 信道数量，当前发送路径最多取 42 个 |
| `+16` | 信道指针数组 | 指向 wiphy 中的 `ieee80211_channel` |
| `+20` | Probe Request IE | 额外 IE 地址 |
| `+32` | IE 长度 | 额外 IE 字节数 |
| `+52` | `wiphy` | 能力/信道表关联 |
| `+56` | 创建时 tick | 扫描时间记录 |
| `+63` | no-CCK 标志 | 扫描速率选项 |

这就是之前直接传公开 `cfg80211_scan_request` 时会超时的原因：公开结构的 `+16` 是
扫描宽度，而预编译库把 `+16` 当成信道指针数组；虽然函数签名能通过编译和链接，运行时
却会读取错误地址。

## 2. 核心类型速查

| 类型 | 含义 | 典型使用 |
|---|---|---|
| `ULONG wdev_id` | 无线设备/软 MAC 设备标识 | 创建接口、扫描、获取 MAC/模式 |
| `int ifidx` / `ifindex` | 虚拟接口索引；本平台通常 `0 = wlan0/STA`、`1 = wlan1/AP` | 连接、密钥、信道、功耗 |
| `enum fc80211_iftype` | 接口类型 | `STATION`、`AP`、`P2P_CLIENT`、`P2P_GO` 等 |
| `enum fc80211_auth_type` | 802.11 认证类型 | Open、Shared Key、FT、EAP、SAE |
| `struct i802_bss` | supplicant 的每接口私有对象 | 包装 API 的第一层私有句柄 |
| `struct wpa_driver_fc80211_data` | fc80211 driver 私有控制块 | 全局/接口状态、扫描和关联状态 |
| `struct wpa_driver_scan_params` | supplicant 扫描参数 | SSID、频率、Probe Request IE、过滤器 |
| `struct wpa_driver_associate_params` | 关联参数 | SSID、BSSID、频点、WPA IE、密钥管理 |
| `struct wpa_driver_auth_params` | 独立认证参数 | BSSID、认证算法、WEP、SAE/FILS 数据 |
| `struct wpa_driver_set_key_params` | 设置/删除密钥参数 | 算法、索引、地址、序列号、密钥数据 |
| `struct wpa_scan_results` | 扫描结果集合 | `res[]` 数组和 `num` 数量 |
| `struct wpa_signal_info` | 信号/噪声/带宽 | RSSI、噪声、频宽、速率等 |
| `struct key_parse` | Renesas cfg80211 密钥解析结果 | `rsdev_cfg80211_*key()` |
| `ra6wx_drv_msg_buf_t` | Renesas 驱动事件消息 | `process_drv_event()`、MLME 事件 |

### 2.1 常用枚举值

`enum fc80211_iftype`：`UNSPECIFIED`、`ADHOC`、`STATION`、`AP`、`AP_VLAN`、`WDS`、`MONITOR`、`MESH_POINT`、`P2P_CLIENT`、`P2P_GO`、`P2P_DEVICE`、`OCB`、`NAN`。

`enum fc80211_auth_type`：`OPEN_SYSTEM`、`SHARED_KEY`、`FT`、`NETWORK_EAP`、`SAE`、`FILS_SK`、`FILS_SK_PFS`、`FILS_PK`。`AUTOMATIC` 只用于“让驱动自行选择”，不能直接作为 Netlink 属性值。

`enum fc80211_key_type`：`GROUP`、`PAIRWISE`、`PEERKEY`。

### 2.2 三个关键私有结构

这些结构虽然不是 RT-Thread WLAN 的数据结构，但很多 API 的参数就是它们的指针。

| 结构 | 关键字段 | 生命周期和作用 |
|---|---|---|
| `struct fc80211_global` | `interfaces`、`if_add_ifindex`、`if_add_wdevid`、`if_add_wdevid_set`、`fc80211_id` | 一个 fc80211 全局实例。先由 `fc80211_global_init()` 创建，再传给 `wpa_driver_fc80211_drv_init()`；所有接口销毁后调用 `fc80211_global_deinit()`。 |
| `struct fc80211_softmac_data` | `list`、`bsss`、`drvs`、`softmac_idx` | 表示一个物理/软 MAC 及其 BSS/driver 链表。AP/STA 共存时用于共享 soft-MAC 能力。 |
| `struct wpa_driver_fc80211_data` | `global`、`ctx`、`ifindex`、`first_bss`、`capa`、`scan_state`、`associated`、`bssid`、`ssid`、`nlmode`、`assoc_freq`、`filter_ssids` | 每个 fc80211 接口的主控制块，保存能力、扫描状态、连接缓存、事件上下文和接口索引。不能通过类型强转或部分初始化伪造。 |
| `struct i802_bss` | `drv`、`ifindex`、`wdev_id`、`ifname`、`addr`、`freq`、`bandwidth`、`ctx`、`softmac_data`、`beacon_set` | supplicant/hostapd 看到的每 BSS 私有对象。包装 API 通常从它得到 `drv`，再进入底层函数。 |

其中 `drv->ctx` 和 `bss->ctx` 是异步事件安全性的关键。事件处理器会沿着 `bss -> drv -> ctx` 找到上层回调，因此 RT-Thread 适配层不能只分配结构体大小相同的内存，而必须使用完整初始化流程，或者绕开这些包装函数使用 Renesas 公共服务 API。

## 3. fc80211 底层命令接口

这些函数直接面向 RA6W1 的 UMAC/firmware 命令或 cfg80211 模拟层。大多数函数要求调用者已经完成 Wi-Fi 打开、接口创建和接口模式设置。

### 3.1 接口、认证、连接和管理帧

| API | 参数/返回值 | 功能和注意事项 |
|---|---|---|
| `fc80211_get_interface_softmac_index(wdev_id, ifidx)` | 返回 soft-MAC 索引，失败通常为负值 | 查询接口属于哪一个物理/软 MAC；AP/STA 共存时用于选择能力和信道集合。 |
| `fc80211_get_interface_mode(wdev_id, ifidx)` | 返回接口模式整数 | 查询当前模式；返回值应转换为 `enum fc80211_iftype`。 |
| `fc80211_get_interface_channel_width(ifidx, sig)` | `sig` 输出 `wpa_signal_info`；返回 0/负错误 | 获取当前接口信道带宽并写入信号信息结构。 |
| `fc80211_get_interface_macaddr(wdev_id, ifidx)` | 返回内部 MAC 指针，失败为 `NULL` | 获取接口 MAC；指针由驱动管理，调用者不要释放或长期保存。 |
| `fc80211_set_interface(wdev_id, ifidx, mode)` | 0 成功，负值失败 | 修改已经存在的虚拟接口模式，例如 STA 与 AP 切换。模式切换可能使当前连接失效。 |
| `fc80211_new_interface(wdev_id, ifidx, ifname, mode)` | `ifname` 为接口名；0/负错误 | 创建新的虚拟接口。`ifidx` 必须符合本 SDK 的接口编号分配。 |
| `fc80211_del_interface(wdev_id, ifidx)` | 0/负错误 | 删除虚拟接口；删除前应停止 AP、断开 STA 并注销事件。 |
| `fc80211_authenticate(ifidx, params, auth_type)` | 认证参数；0/负错误 | 只执行 802.11 Authentication 阶段，适用于 supplicant 自己实现 SME 的场景。普通连接通常使用 `fc80211_connect()` 或 `wpa_driver_fc80211_associate()`。 |
| `fc80211_deauthenticate(ifidx, bssid, reason_code, local_state_change)` | BSSID 可为空；0/负错误 | 向对端发送 Deauthentication 并清理认证状态。`local_state_change` 表示是否只改变本地状态。 |
| `fc80211_associate(ifidx, params)` | 关联参数；0/负错误 | 执行 Association 阶段，不负责完整的扫描和高层 WPA 状态机。 |
| `fc80211_connect(ifidx, params, auth_type, privacy)` | 关联参数、认证类型、是否启用隐私；0/负错误 | 一次性执行驱动侧连接流程，适合固件/驱动完成认证和关联的模式。 |
| `fc80211_disconnect(ifidx, reason_code)` | 16 位 reason；0/负错误 | 断开当前连接并通知 MAC/固件。 |
| `fc80211_chk_deauth_send_done(ifidx)` | 返回完成/未完成或负错误 | 查询 Deauthentication 帧是否发送完成，主要用于 P2P GO inactivity。 |
| `fc80211_tx_mgmt(wdev_id, ifidx, freq, wait, buf, buf_len, offchanok, no_cck, no_ack, cookie)` | 管理帧和发送选项；`cookie` 输出事务标识 | 在指定频率发送 802.11 管理帧；`wait`/off-channel/ACK 选项控制发送行为。 |
| `fc80211_frame_wait_cancel(ifindex, cookie)` | cookie；0/负错误 | 取消等待管理帧响应的事务。 |
| `fc80211_remain_on_channel(ifindex, freq, duration)` | 返回 64 位 cookie | 让接口在指定信道停留一段时间，常用于 P2P/Action frame。返回 cookie 不是错误码。 |
| `fc80211_cancel_remain_on_channel(ifidx, cookie)` | cookie；0/负错误 | 取消 `remain_on_channel`。 |

### 3.2 P2P、射频和连接辅助

| API | 参数/返回值 | 功能和注意事项 |
|---|---|---|
| `fc80211_p2p_go_ps_on_off(ifindex, p2p_ps_status)` | 仅 `CONFIG_P2P_POWER_SAVE && CONFIG_P2P_UNUSED_CMD`；0/负错误 | 打开或关闭 P2P GO 电源保存。当前头文件条件编译，普通构建可能没有该符号。 |
| `fc80211_start_p2p_dev(wdev_id, ifidx)` | 0/负错误 | 启动 P2P Device 类型接口。P2P Device 不是普通数据网卡。 |
| `fc80211_stop_p2p_dev(wdev_id, ifidx)` | 0/负错误 | 停止 P2P Device 接口。 |
| `fc80211_set_noa(ifindex, count, duration, interval, start)` | 位于 `#if 0`，默认不可用 | 配置 P2P Notice of Absence；头文件保留声明但当前代码未启用。 |
| `fc80211_set_opp_ps(ifindex, ctwindow)` | 位于 `#if 0`，默认不可用 | 配置 P2P Opportunistic Power Save；当前未使用。 |
| `fc80211_enable_rssi_report(ifindex, min, max)` | RSSI 阈值；0/负错误 | 配置固件在 RSSI 越过范围时上报事件。 |
| `fc80211_set_cqm_rssi(ifindex, thresholds, hysteresis)` | 阈值和迟滞；0/负错误 | 配置连接质量监视器（CQM）RSSI 事件。 |
| `fc80211_get_rtctimegap(ifindex, rtctimegap)` | 输出时间差；0/负错误 | 获取驱动/RTC 时间基准之间的间隔，供时间戳或功耗恢复使用。 |
| `fc80211_get_empty_rid()` | 返回空闲 RID 或负值 | 获取一个可用的请求/资源标识。 |
| `fc80211_set_app_keepalivetime(tid, sec, callback_func)` | 流标识、秒数、回调；0/负错误 | 设置应用层 keep-alive 定时器；超时后通过回调报告。 |
| `fc80211_channel_switch(ifindex, settings)` | `csa_settings`；0/负错误 | 执行 Channel Switch Announcement 相关的信道切换。 |
| `fc80211_update_ft_ies(ifindex, mdid, ies, ies_len)` | Mobility Domain 和 FT IEs；0/负错误 | 更新 802.11r Fast BSS Transition 所需的信息元素；`ies == NULL` 可表示清除。 |
| `fc80211_external_auth_status(ifindex, params)` | 外部认证结果；0/负错误 | 将 SAE/FILS 等外部认证结果回送给驱动。 |

### 3.3 注册域、能力和扫描

| API | 参数/返回值 | 功能和注意事项 |
|---|---|---|
| `fc80211_get_reg(wdev_id, ifidx, handler, arg)` | 回调遍历法规信息；0/负错误 | 获取当前 regulatory domain；头文件标注为尚未定义，不能假设所有固件支持。 |
| `fc80211_req_set_reg(ifidx, alpha2)` | 两字母国家码；0/负错误 | 请求改变注册域。Renesas 适配中通常优先使用 `wpa_driver_fc80211_set_country()` 或 `WIFI_SetCountryCode()`。 |
| `fc80211_get_protocol_features(ifidx, feat)` | 输出能力位图；0/负错误 | 查询驱动支持的协议特性。 |
| `fc80211_get_softmac(wdev_id, ifidx, arg)` | `arg` 指向驱动定义的 soft-MAC 信息 | 获取物理层/软 MAC 能力，结构内容由实现决定。 |
| `fc80211_get_scan(wdev_id, ifidx, arg)` | `arg` 通常为内部扫描结果收集结构；0/负错误 | 从 UMAC 读取最近一次扫描的 BSS 列表；不会自动分配 RT-Thread WLAN 结构。 |
| `fc80211_trigger_scan(wdev_id, ifidx, params)` | 扫描参数；0/负错误 | 触发异步扫描；完成后通常由事件路径通知。 |
| `fc80211_abort_scan(wdev_id, ifindex)` | 0/负错误 | 中止正在进行的扫描。 |
| `fc80211_free_umac_scan_results(wdev_id, ifindex)` | 0/负错误 | 释放 UMAC 扫描结果缓存；是否必须调用取决于当前扫描结果实现。 |
| `fc80211_sched_scan(ifidx, params, interval)` | 扫描参数和周期；0/负错误 | 启动后台周期扫描。 |
| `fc80211_stop_sched_scan(ifidx)` | 0/负错误 | 停止后台周期扫描。 |
| `fc80211_set_tx_bitrate_mask(ifidx, disabled)` | 是否禁用 11b 速率；0/负错误 | 设置 TX bitrate mask；当前实现由 `fc80211_disable_11b_rates()` 使用，`disabled` 非零时禁用 802.11b 速率。 |
| `fc80211_dump_survey(ifindex, idx, get_survey)` | 仅 `CONFIG_ACS`；信道索引和输出结构 | 读取信道 survey 数据，供 ACS（自动信道选择）使用。 |
| `fc80211_unexpected_frame(ifidx)` | 0/负错误 | 通知驱动遇到了意外管理帧/异常帧，具体处理由固件实现。 |
| `fc80211_register_action(wdev_id, ifindex, type, match, match_len)` | 注册 Action frame 类型和匹配前缀；0/负错误 | 将特定 Action frame 交给上层事件路径。 |

### 3.4 密钥、信道和站点

| API | 参数/返回值 | 功能和注意事项 |
|---|---|---|
| `fc80211_get_key(ifindex, seq, key_idx, mac_addr)` | 输出序列号；0/负错误 | 获取指定密钥的序列号，通常用于 GTK/Replay Counter。 |
| `fc80211_new_key(ifidx, addr, key, key_len, seq, seq_len, key_idx, set_tx, alg)` | 密钥、序列号、索引、算法；0/负错误 | 向驱动添加密钥。广播地址表示组密钥，普通地址表示单播密钥。 |
| `fc80211_del_key(ifidx, addr, seq, seq_len, key_idx, set_tx)` | 密钥标识；0/负错误 | 删除密钥。具体地址语义遵循 `wpa_driver_set_key_params`。 |
| `fc80211_set_key(ifidx, addr, alg, key_idx)` | 算法和索引；0/负错误 | 设置/选择默认密钥或密钥索引。 |
| `fc80211_set_channel(ifidx, freq, set_chan)` | 频率参数和是否真正切换；0/负错误 | 设置接口信道；AP 启动、CSA、扫描恢复时可能调用。 |
| `fc80211_get_channel(wdev, chandef)` | 输出 `cfg80211_chan_def`；0/负错误 | 查询当前信道定义，包括主信道、带宽和中心频率。 |
| `fc80211_set_wnm_sleep_mode(oper, ifindex, intval)` | 仅 `CONFIG_WNM_SLEEP_MODE`；0/负错误 | 设置/退出 802.11v WNM-Sleep 状态；`oper` 和间隔值由 WNM 定义。 |
| `fc80211_set_bss(ifindex, cts, preamble, slot, ht_opmode, ap_isolate, rates, rates_len)` | BSS MAC 参数；0/负错误 | 设置 CTS 保护、短前导、短时隙、HT 操作模式、AP 隔离和基本速率。 |
| `fc80211_set_mac_acl(ifidx, params)` | hostapd ACL 参数；0/负错误 | 配置 AP 的 MAC 地址访问控制列表。 |
| `fc80211_new_station(ifidx, params, arg)` | AP 站点参数；0/负错误 | 在 AP 中增加关联站点。 |
| `fc80211_set_station(ifidx, mac_addr, flags)` | 位于 `#if 0`，默认不可用 | 头文件保留的旧版站点状态设置声明；当前构建应使用 `rsdev_cfg80211_set_station()` 或 `fc80211_new_station()`。 |
| `fc80211_probe_client(ifidx, addr)` | 站点 MAC；0/负错误 | 向 AP 下的客户端发送探测，用来确认客户端是否仍在线。 |
| `fc80211_set_softmac(ifidx, queue, aifs, cwmin, cwmax, burst_time)` | EDCA/队列参数；0/负错误 | 设置指定发送队列的竞争参数。 |
| `fc80211_set_softmac_rts(ifidx, rts)` | RTS 阈值；0/负错误 | 设置 RTS threshold。 |
| `fc80211_get_softmac_rts(ifidx)` | 返回 RTS 阈值或负错误 | 读取 RTS threshold。 |
| `fc80211_set_softmac_frag(ifidx, frag)` | 仅 `__FRAG_ENABLE__`；0/负错误 | 设置 802.11 分片阈值。 |
| `fc80211_set_softmac_retry(ifindex, retry, retry_long)` | 短/长重试次数；0/负错误 | 设置 MAC 重传限制。 |
| `fc80211_get_softmac_retry(ifindex, retry_long)` | 返回重试次数或负错误 | 读取短重试或长重试限制。 |
| `fc80211_set_cqm(ifidx, threshold, hysteresis)` | CQM 阈值；0/负错误 | 配置通用连接质量监视，和 RSSI 专用接口相比参数更抽象。 |

### 3.5 功耗、AMPDU、AP 和其他底层控制

| API | 参数/返回值 | 功能和注意事项 |
|---|---|---|
| `fc80211_set_rekey_offload(ifidx, kek, kck, replay_ctr)` | 位于 `#if 0`，默认不可用 | 配置驱动侧 GTK rekey offload；当前 SDK 未启用。 |
| `rs_cfg80211_radar_start(ifindex, freq)` | 频率参数；0/负错误 | 启动 DFS 雷达检测；名称中的 `rs` 表示 Renesas/RW cfg80211 适配。 |
| `fc80211_ctrl_bridge(bridge_control)` | `true`/`false`；0/负错误 | 控制驱动内部桥接/转发行为，AP 隔离实现会使用。 |
| `fc80211_set_power_save(ifidx, ps_state, timeout)` | 功耗状态和超时；0/负错误 | 设置接口功耗策略。 |
| `fc80211_ctrl_ampdu_rx(ampdu_rx_control)` | 无返回值 | 打开/关闭 AMPDU RX 相关控制。 |
| `fc80211_set_sta_power_save(ifindex, pwrsave_mode)` | 无返回值 | 设置 STA 侧 power-save 模式。 |
| `fc80211_set_ampdu_flag(mode, val)` | 无返回值 | 设置全局/模式相关 AMPDU 标志。 |
| `fc80211_get_ampdu_flag(mode)` | 返回标志值 | 读取 AMPDU 标志。 |
| `fc80211_set_roaming_flag(mode)` | 仅 `CONFIG_SIMPLE_ROAMING`；无返回值 | 设置简单漫游开关/模式。 |
| `fc80211_none_station(ifindex)` | 仅 `CONFIG_AP && CONFIG_AP_NONE_STA`；0/负错误 | AP 没有站点时通知/处理相关状态。 |
| `fc80211_radar_detect(ifidx, freq)` | 仅 `CONFIG_AP`；0/负错误 | AP 模式下执行雷达检测。 |
| `fc80211_set_ap_power(ifindex, type, power, get_power)` | 仅 `CONFIG_AP_POWER`；0/负错误 | 设置或读取 AP 发射功率。`get_power` 用于返回读取结果。 |
| `fc80211_sta_null_send(ifindex, mac_addr)` | 仅 `CONFIG_AP`；0/负错误 | 向指定 STA 发送 Null data，用于 keep-alive/状态探测。 |

## 4. 全局状态、事件和属性解析辅助 API

### 4.1 全局对象和状态判断

| API | 功能 |
|---|---|
| `driver_fc80211_process_global_ev(drv_msg_buf)` | 处理没有明确归属单个 BSS 的全局驱动消息，并分发到对应 fc80211 driver。由驱动事件任务调用。 |
| `fc80211_global_deinit(priv)` | 释放 `fc80211_global_init()` 产生的全局私有状态。 |
| `fc80211_global_init()` | 创建全局 fc80211 状态，返回指针；失败返回 `NULL`。通常由 `wpa_driver_fc80211_ops.global_init` 使用。 |
| `is_ap_interface(nlmode)` | 返回非零表示模式是 AP 类接口。 |
| `is_sta_interface(nlmode)` | 返回非零表示模式是 STA 类接口。 |
| `is_p2p_net_interface(nlmode)` | 仅 `CONFIG_P2P`；判断是否为 P2P Client/GO 等网络接口。 |
| `fc80211_mark_disconnected(drv)` | 清除 driver 的已关联状态、BSSID 等缓存，通常在主动断开或重新认证前调用。 |
| `fc80211_get_softmac_index(bss)` | 从 BSS 的 `wdev_id/ifindex` 查询 soft-MAC 索引。 |
| `fc80211_get_ifmode(bss)` | 查询 BSS 当前接口模式。 |
| `fc80211_get_macaddr(bss)` | 获取 BSS 的 MAC；失败返回负值。 |
| `fc80211_get_softmac_data_ap(bss)` | 获取或创建 AP 软 MAC 共享数据对象；返回 `NULL` 表示失败。 |
| `fc80211_put_softmac_data_ap(bss)` | 释放/减少 AP 软 MAC 共享数据引用。 |
| `fc80211_addr_in_use(global, addr)` | 仅 `CONFIG_P2P`；判断地址是否已被其他接口使用。 |
| `fc80211_p2p_interface_addr(drv, new_addr)` | 仅 `CONFIG_P2P`；生成可用的 P2P 接口地址，写入 `new_addr`。 |
| `fc80211_wdev_handler(arg)` | 处理 wireless device 异步消息；通常由 SDK 事件线程/消息队列调用。 |
| `dump_ifidx(drv)` | 调试输出当前 driver 管理的接口索引。 |
| `fc80211_cmd_to_string(cmd)` | 将 `enum fc80211_commands` 转为可读字符串；返回静态字符串，不要释放。 |

### 4.2 Netlink 属性解析函数

这些函数操作头文件中定义的轻量 `struct nlattr`，用于解析 fc80211 消息属性，不负责分配属性内存。

| API | 返回值/作用 |
|---|---|
| `nla_get_u8(nla)` | 读取属性负载为 `u8`。 |
| `nla_get_u16(nla)` | 读取属性负载为 `u16`。 |
| `nla_get_u32(nla)` | 读取属性负载为 `u32`。 |
| `nla_get_u64(nla)` | 读取属性负载为 `u64`。 |
| `nla_len(nla)` | 返回属性负载长度；头文件中重复声明了两次，实际是同一个函数。 |
| `nla_data(nla)` | 返回属性负载地址；指针指向消息缓冲区，不要释放。 |
| `nla_ok(nla, remaining)` | 判断当前属性是否在剩余消息范围内有效。 |
| `nla_next(nla, remaining)` | 移动到下一个属性并更新剩余长度。 |

对应的宏 `nla_for_each_attr`、`nla_for_each_nested`、`NLA_ALIGN` 等只负责遍历和对齐；使用时必须保证消息缓冲区长度可信，否则可能越界读取。

### 4.3 平台注册域和运行状态辅助函数

| API | 功能 |
|---|---|
| `get_run_mode()` | 返回 Renesas 工程运行模式，具体枚举由平台定义。 |
| `RM_PMGR_W_dpm_is_enabled()` | 仅 `CFG_PMGR`；查询动态电源管理是否启用。 |
| `RM_PMGR_W_dpm_is_wakeup()` | 仅 `CFG_PMGR`；查询当前是否由低功耗状态唤醒。 |
| `ra6w1_regdb_data_init(country)` | 用国家码初始化 RA6W1 regulatory database。 |
| `ra6w1_regdb_get_ch_range_by_country_n_band(country, band, min_ch, max_ch, ch_bitmap_5g, exclude_flags)` | 查询指定国家和频段的允许信道范围/5 GHz 位图；输出通过指针返回。 |
| `ra6w1_regdb_get_init_country(country)` | 输出启动时国家码；成功通常返回 `pdTRUE`。 |
| `wpa_alg_to_cipher_suite(alg, key_len)` | 把 `enum wpa_alg` 和密钥长度转换为 fc80211 cipher suite；无效算法返回 0。 |
| `sta_flags_fc80211(flags)` | 把 hostapd STA 标志转换为 fc80211 STA 标志位。 |

## 5. `rsdev_cfg80211_*` Renesas cfg80211 适配接口

这些接口位于 supplicant 与 Renesas RW/rwnx MAC 驱动之间。它们比 `fc80211_*` 更接近“配置动作”，但多数仍使用 supplicant 私有控制块。

| API | 功能和关键参数 |
|---|---|
| `__cfg80211_disconnected(ifindex, reason, ie, ie_len, locally_generated)` | 向 cfg80211 状态层报告断开；`locally_generated` 区分主动断开和空口丢链路。 |
| `rsdev_cfg80211_associate(drv, param)` | 根据 `wpa_driver_associate_params` 发起关联；处理 WPA IE、频点、BSSID 等。 |
| `rsdev_cfg80211_del_interface(wdev_id, ifindex)` | 删除 Renesas cfg80211 接口。 |
| `rsdev_cfg80211_del_station(drv, mac_addr, deauth, reason_code)` | AP 删除站点，可选择先发 deauth。 |
| `rsdev_cfg80211_new_interface(wdev_id, ifindex, ifname, type)` | 创建并注册新的网络接口。 |
| `rsdev_cfg80211_new_station(drv, arg, upd)` | 添加 AP 站点及其标志更新信息。 |
| `rsdev_cfg80211_scan(wdev_id, drv, params)` | 根据 host 扫描参数触发扫描；支持 SSID、频率、额外 IE 和过滤器。当前预编译库还会读取 `drv` 的 `ifindex/nlmode/filter_ssids` 固定偏移，以及 `params` 的 host scan 固定偏移；不使用 supplicant 时必须提供 ABI 对齐的本地适配结构，不能只传一个公开 `cfg80211_scan_request`。 |
| `rsdev_cfg80211_set_default_mgmt_key(ifindex, key_p)` | 设置默认管理帧密钥（IGTK/BIGTK）。 |
| `rsdev_cfg80211_set_default_beacon_key(ifindex, key_p)` | 设置默认 Beacon 管理密钥。 |
| `rsdev_cfg80211_add_key(ifindex, key_p, addr)` | 增加组密钥或单播密钥；`addr == NULL` 常用于广播密钥。 |
| `rsdev_cfg80211_set_key(ifindex, key_p, mac_addr)` | 更新已有 key 的 TX/默认属性。 |
| `rsdev_cfg80211_del_key(ifindex, key_p, addr)` | 删除 key。 |
| `rsdev_cfg80211_set_pmk(pmk_conf)` | 设置 PMK/PMKSA 所需的主密钥材料；这是敏感数据，不能打印。 |
| `rsdev_cfg80211_set_station(drv, arg, upd)` | 修改已存在站点的认证/授权/QoS 状态。 |
| `rsdev_cfg80211_tx_mgmt(drv, freq, wait, buf, buf_len, offchanok, no_cck, no_ack, cookie, csa_offs, csa_offs_len)` | 发送管理帧，并处理 off-channel、ACK、CSA 偏移和 cookie。 |
| `rsdev_tx_control_port(dest, proto, buf, len, no_encrypt, cookie)` | 通过驱动 control port 发送 EAPOL/控制端口报文。 |

**直接调用建议：**如果调用者不是 supplicant，不能只伪造一个 `drv` 指针。可以参考 [`wifi_fc80211_demo.c`](/home/rain/renesas/rt-thread/bsp/renesas/ra6w1-ek/wifi/app/wifi_fc80211_demo.c) 的最小扫描链路，但必须自行实现生命周期、互斥、事件和缓存，并确认预编译库的结构体 ABI 没有变化。该 demo 只覆盖初始化和扫描，不等价于完整的 WPA 连接状态机。

## 6. supplicant/hostapd 包装 API

### 6.1 初始化、模式和能力

| API | 功能和调用关系 |
|---|---|
| `wpa_driver_fc80211_drv_init(ctx, ifname, global_priv, hostapd, set_addr)` | 创建 `wpa_driver_fc80211_data` 和首个 `i802_bss`，设置接口名、上下文和 hostapd 标志，并调用内部初始化。`global_priv` 不能为 `NULL`；失败返回 `NULL`。 |
| `wpa_driver_fc80211_init(ctx, ifname, global_priv)` | `drv_init(..., hostapd=0, set_addr=NULL)` 的 supplicant 简化入口。 |
| `wpa_driver_fc80211_capa(drv)` | 读取硬件/固件能力并填充 `drv->capa`；返回 0/负错误。 |
| `wpa_driver_fc80211_get_capa(priv, capa)` | `wpa_driver_ops.get_capa` 入口，将私有句柄转换为 BSS/driver 后调用能力查询。 |
| `get_fc80211_protocol_features(drv)` | 返回协议特性位图。 |
| `wpa_driver_fc80211_get_hw_feature_data(priv, num_modes, flags)` | 返回分配的硬件模式/信道/速率数组；调用者负责按 supplicant 约定释放。 |
| `wpa_driver_fc80211_postprocess_modes(modes, num_modes)` | 对硬件模式数据做 Renesas 平台后处理；可能返回新的模式指针。 |
| `wpa_driver_fc80211_if_type(type)` | 将 `enum wpa_driver_if_type` 转换为 `enum fc80211_iftype`。 |
| `fc80211_set_mode(bss, ifindex, mode)` | 设置 BSS 模式并同步 `drv->nlmode`；这是包装层常用的模式切换入口。 |
| `i802_deinit(priv)` | hostapd/i802 私有接口销毁。 |
| `driver_fc80211_deinit(priv)` | `wpa_driver_ops.deinit` 入口，调用 BSS 级清理。 |
| `wpa_driver_fc80211_resume(priv)` | 从挂起/低功耗恢复驱动状态。 |
| `fc80211_get_radio_name(priv)` | 返回物理射频名称，通常是静态字符串。 |
| `fc80211_set_param(priv, param)` | 设置驱动私有字符串参数；当前实现支持范围由 `.c` 文件决定。 |
| `wpa_driver_fc80211_set_country(priv, alpha2)` | 使用 RA6W1 regdb 设置两字母国家码，并更新 MAC 信道/功率信息。 |
| `wpa_driver_fc80211_get_country(priv, alpha2)` | 输出当前国家码，成功返回 0。 |

### 6.2 管理帧订阅和扫描

| API | 功能和注意事项 |
|---|---|
| `fc80211_register_action_frame(bss, match, match_len)` | 在 BSS 上注册 Action frame 匹配规则。 |
| `fc80211_mgmt_subscribe_non_ap(bss)` | 订阅 STA/P2P Client 侧管理帧。 |
| `fc80211_register_spurious_class3(bss)` | 注册 Class 3 frame 异常报告。 |
| `fc80211_mgmt_subscribe_ap(bss)` | 订阅 AP 侧管理帧。 |
| `fc80211_mgmt_subscribe_ap_dev_sme(bss)` | AP device SME 模式下的管理帧订阅。 |
| `fc80211_mgmt_unsubscribe(bss, reason)` | 注销所有管理帧订阅并记录原因。 |
| `fc80211_set_p2pdev(bss, start)` | 仅 `CONFIG_P2P`；启动/停止 P2P device 相关管理通道。 |
| `fc80211_del_p2pdev(bss)` | 仅 `CONFIG_P2P`；删除 P2P device。 |
| `wpa_driver_fc80211_scan(bss, params)` | supplicant 扫描入口：保存过滤 SSID、调用 `rsdev_cfg80211_scan`、设置扫描状态并安排超时。返回 0/负错误。 |
| `driver_fc80211_scan2(priv, params)` | `wpa_driver_ops.scan2` 薄包装，内部调用 `wpa_driver_fc80211_scan`。 |
| `wpa_driver_fc80211_scan_timeout(timeout, drv, ctx)` | 扫描没有及时产生硬件完成事件时，触发结果读取/完成处理。它不是普通 RT-Thread 定时器接口。 |
| `fc80211_get_scan_results(drv)` | 读取 UMAC 扫描 BSS，分配并返回 `struct wpa_scan_results`。调用者负责释放。 |
| `wpa_driver_fc80211_get_scan_results(priv)` | `get_scan_results2` 包装入口。 |
| `wpa_driver_fc80211_free_umac_scan_results(priv)` | 释放 UMAC 扫描缓存的包装入口。 |
| `fc80211_dump_scan(drv)` | 调试输出扫描状态/结果，不改变扫描结果所有权。 |
| `wpa_driver_fc80211_del_beacon(drv)` | 删除已设置的 Beacon，主要用于 AP deinit 或重新配置。 |

### 6.3 认证、关联、断开和密钥

| API | 功能和注意事项 |
|---|---|
| `wpa_driver_fc80211_auth(bss, params)` | supplicant SME 模式的独立认证入口；会复制认证参数、设置 STA/P2P 模式并发送认证请求。 |
| `driver_fc80211_authenticate(priv, params)` | `wpa_driver_ops.authenticate` 包装入口。 |
| `wpa_auth_type_txt(type)` | 将认证枚举转为静态文本，如 `OPEN_SYSTEM`、`SAE`；用于日志。 |
| `fc80211_copy_auth_params(drv, params)` | 将认证参数复制进 driver 私有缓存；会分配/替换认证 IE，调用者仍负责原始参数生命周期。 |
| `wpa_driver_fc80211_try_connect(drv, params)` | 直接调用 `rsdev_cfg80211_associate` 尝试连接一次。 |
| `wpa_driver_fc80211_connect(drv, params)` | 连接包装；遇到 `-EALREADY` 时会先主动断开，再重试。 |
| `wpa_driver_fc80211_associate(priv, params)` | `wpa_driver_ops.associate` 总入口。STA 模式走连接路径，AP 参数走 AP 初始化路径。 |
| `driver_fc80211_deauthenticate(priv, addr, reason)` | `wpa_driver_ops.deauthenticate` 入口；根据 driver 是否支持 SME 选择 MLME deauth 或完整断开。 |
| `wpa_driver_fc80211_deauthenticate(bss, addr, reason)` | BSS 级 deauthenticate 包装。 |
| `wpa_driver_fc80211_disconnect(drv, reason)` | 标记本地断开并调用 `rsdev_cfg80211_disconnect`；会设置忽略重复本地断开事件的标志。 |
| `wpa_driver_fc80211_set_key(bss, params)` | 解析 `wpa_driver_set_key_params`，转换 cipher suite/key flag，再调用 `rsdev_cfg80211_add_key/set_key/del_key`。这是 WPA 四次握手密钥下发的关键入口。 |
| `driver_fc80211_set_key(priv, params)` | `wpa_driver_ops.set_key` 包装入口。 |
| `fc80211_set_conn_keys(params, privacy)` | 根据 WEP key、WPS privacy、pairwise cipher 判断连接是否需要隐私保护，并写回 `privacy`。 |
| `wpa_driver_fc80211_set_supp_port(priv, authorized)` | 打开/关闭 supplicant control port，常用于 802.1X/WPA 认证完成前后。 |
| `fc80211_tx_control_port(priv, dest, proto, buf, len, no_encrypt)` | control port 发送包装函数；无 cookie 版本。 |
| `fc80211_send_eapol_data(bss, addr, data, data_len)` | 仅 `IEEE8021X_EAPOL`；发送 EAPOL 数据。 |
| `wpa_driver_fc80211_hapd_send_eapol(priv, addr, data, data_len, encrypt, own_addr, flags)` | 仅 `IEEE8021X_EAPOL`；hostapd AP 侧 EAPOL 发送入口。 |

### 6.4 AP、站点和接口管理

| API | 功能和注意事项 |
|---|---|
| `wpa_driver_fc80211_set_ap(priv, params)` | 设置 Beacon/Probe Response、SSID、WPA 参数、隐藏 SSID 等 AP 模板信息。 |
| `fc80211_setup_ap(bss)` | 仅 `CONFIG_AP`；建立 AP 管理帧订阅和 AP 侧驱动状态。 |
| `fc80211_teardown_ap(bss)` | 仅 `CONFIG_AP`；拆除 AP 状态。 |
| `wpa_driver_fc80211_ap(drv, params)` | 仅 `CONFIG_AP`；处理 `associate()` 收到的 AP 模式参数，设置模式、信道并启动 AP 相关流程。 |
| `wpa_driver_fc80211_set_ap_isolate(priv, isolate)` | 仅 `CONFIG_AP_ISOLATION`；通过 `fc80211_ctrl_bridge` 控制 AP 客户端隔离。 |
| `wpa_driver_fc80211_set_acl(priv, params)` | 设置 hostapd MAC ACL；能力不足时返回 `-ENOTSUPP`。 |
| `wpa_driver_fc80211_sta_add(priv, params)` | 增加 AP 客户端站点。 |
| `wpa_driver_fc80211_sta_remove(bss, addr, deauth, reason)` | 从 AP 移除站点，可选择先发 deauth。 |
| `driver_fc80211_sta_remove(priv, addr)` | `sta_remove` 简化包装，使用默认 deauth 参数。 |
| `wpa_driver_fc80211_sta_set_flags(priv, addr, total_flags, flags_or, flags_and)` | 修改站点授权、认证、WME/MFP 等标志。 |
| `fc80211_bss_set(bss, cts, preamble, slot, ht_opmode, ap_isolate, basic_rates)` | 将 hostapd BSS 参数转换成 fc80211 基本速率和保护设置。 |
| `fc80211_remove_iface(drv, ifidx)` | 从 driver 内部接口列表移除接口并完成相应状态清理。 |
| `fc80211_create_iface_once(drv, ifname, iftype, addr, wds, handler, arg)` | 创建一次接口；若接口已存在通常返回错误。 |
| `fc80211_create_iface(drv, ifname, iftype, addr, wds, handler, arg, use_existing)` | 创建接口或按 `use_existing` 复用已有接口。 |
| `wpa_driver_fc80211_if_add(priv, type, ifname, addr, bss_ctx, drv_priv, force_ifname, if_addr, bridge, use_existing)` | `wpa_driver_ops.if_add` 入口，创建 supplicant/hostapd 需要的虚拟接口，并输出 driver 私有指针和最终地址。 |
| `driver_fc80211_if_remove(priv, type, ifname)` | `if_remove` 包装入口。 |
| `i802_check_bridge(drv, bss, brname, ifname)` | 检查接口是否已加入指定桥；嵌入式工程没有 Linux bridge 时通常不使用。 |
| `i802_init(hapd, params)` | hostapd 专用初始化入口。 |

### 6.5 数据帧、Action frame、P2P 和功耗

| API | 功能和注意事项 |
|---|---|
| `wpa_driver_fc80211_send_frame(bss, data, len, encrypt, noack, freq, no_cck, offchanok, wait_time, csa_offs, csa_offs_len)` | 发送原始管理/数据控制帧；由驱动负责频率、加密、ACK 和 off-channel 处理。 |
| `wpa_driver_fc80211_send_mlme(bss, data, data_len, noack, freq, no_cck, offchanok, wait_time, csa_offs, csa_offs_len, no_encrypt)` | 发送 MLME 管理帧；会根据帧类型决定是否调用 control port 或普通发送路径。 |
| `fc80211_send_frame(priv, data, data_len, encrypt)` | 简化的原始帧发送包装。 |
| `driver_fc80211_send_mlme(priv, data, data_len, noack, freq, csa_offs, csa_offs_len, no_encrypt, wait)` | `wpa_driver_ops.send_mlme` 包装入口。 |
| `fc80211_register_action_frame(bss, match, match_len)` | 注册 Action frame 匹配规则；见第 6.2 节。 |
| `wpa_driver_fc80211_send_action(bss, freq, wait_time, dst, src, bssid, data, data_len, no_cck)` | 发送 P2P/Action frame，并记录发送 cookie。 |
| `driver_fc80211_send_action(priv, freq, wait_time, dst, src, bssid, data, data_len, no_cck)` | `send_action` 包装入口。 |
| `wpa_driver_fc80211_send_action_cancel_wait(priv)` | 取消正在等待 Action frame 响应的操作。 |
| `wpa_driver_fc80211_remain_on_channel(priv, freq, duration)` | `remain_on_channel` 包装入口。 |
| `wpa_driver_fc80211_cancel_remain_on_channel(priv)` | 取消当前 remain-on-channel。 |
| `fc80211_sta_pwrsave(priv, pwrsave_mode)` | 仅 `CONFIG_STA_POWER_SAVE`；设置 STA 省电模式。 |
| `fc80211_ap_pwrsave(priv, ps_state, timeout)` | 仅 `CONFIG_AP`；设置 AP 电源策略。 |
| `fc80211_addba_reject(priv, addba_reject)` | 仅 `CONFIG_AP`；控制 AMPDU RX/ADDBA reject。 |
| `driver_fc80211_ap_none_station(priv)` | 仅 `CONFIG_AP_NONE_STA`；通知 AP 当前没有客户端。 |
| `driver_fc80211_set_ap_power(priv, type, power, get_power)` | 仅 `CONFIG_AP_POWER`；AP 发射功率包装入口。 |
| `i802_set_freq(priv, freq)` | 设置 i802 接口频率/信道。 |
| `i802_set_rts(priv, rts)` / `i802_get_rts(priv)` | 设置/读取 RTS 阈值。 |
| `i802_set_retry(priv, retry, retry_long)` / `i802_get_retry(priv, retry_long)` | 设置/读取重试次数。 |
| `i802_set_tx_queue_params(priv, queue, aifs, cwmin, cwmax, burst_time)` | 设置发送队列 EDCA 参数。 |
| `i802_flush(priv)` | 刷新驱动队列/状态。 |
| `i802_set_ap_power(bss, type, power, get_power)` | 仅 `CONFIG_AP_POWER`；BSS 级 AP 功率设置。 |
| `i802_chk_deauth_send_done(priv)` | 查询 deauth 是否完成。 |
| `i802_chk_keep_alive(priv, addr)` | 仅 `CONFIG_AP`；查询/触发客户端 keep-alive。 |

### 6.6 统计、信号、survey 和杂项

| API | 功能和注意事项 |
|---|---|
| `wpa_driver_fc80211_get_bssid(priv, bssid)` | 输出当前关联 BSSID。 |
| `wpa_driver_fc80211_get_ssid(priv, ssid)` | 输出当前关联 SSID；返回长度或 0/负值取决于实现约定。 |
| `fc80211_get_assoc_freq(drv)` | 返回当前关联频率（MHz），未关联时通常为 0；读取扫描结果中的关联频点并更新 `drv->assoc_freq`；可用于 Action frame、漫游和链路诊断。 |
| `fc80211_get_link_signal(drv, sig)` | 读取当前链路 RSSI、速率、频率等信号信息。 |
| `fc80211_get_link_noise(drv, sig_change)` | 读取噪声/信号变化数据。 |
| `fc80211_signal_monitor(priv, threshold, hysteresis)` | 配置/启停信号阈值监视。 |
| `wpa_driver_fc80211_shared_freq(priv)` | 查询/判断与其他接口共享频率的状态。 |
| `fc80211_get_dump_survey(priv, channel)` | 读取指定信道 survey；底层使用 `fc80211_dump_survey`。 |
| `fc80211_get_station(ifindex, sta_mac, cmd_type, sinfo)` | 获取指定站点统计；STA 模式可用 BSSID，AP 模式可用客户端 MAC。 |
| `i802_read_sta_data(bss, data, addr)` | 读取 hostapd 格式的站点统计数据。 |
| `driver_fc80211_read_sta_data(priv, data, addr)` | `read_sta_data` 包装入口。 |
| `i802_sta_clear_stats(priv, addr)` | 清除站点统计。 |
| `i802_sta_deauth(priv, own_addr, addr, reason)` | hostapd 侧站点 deauth。 |
| `i802_sta_disassoc(priv, own_addr, addr, reason)` | hostapd 侧站点 disassoc。 |
| `wpa_driver_fc80211_get_macaddr(priv)` | `get_mac_addr` 包装入口，返回驱动拥有的 MAC 指针。 |
| `driver_fc80211_probe_req_report(priv, report)` | 开关 Probe Request 上报。 |
| `driver_fc80211_get_rtctimegap(priv, rtctimegap)` | `fc80211_get_rtctimegap` 包装入口。 |
| `driver_fc80211_send_external_auth_status(priv, params)` | `fc80211_external_auth_status` 包装入口。 |
| `fc80211_set_ampdu_mode(mode, val)` / `fc80211_get_ampdu_mode(mode)` | AMPDU 模式设置/读取包装。 |
| `i802_get_seqnum(iface, priv, addr, idx, seq)` | 读取指定密钥/站点的序列号。 |

## 7. 事件处理 API

这些函数把 Renesas 驱动消息转换成 supplicant 事件。它们的共同点是需要有效的 `drv`、`bss` 和事件上下文；不应在没有事件宿主的 RT-Thread 适配层中直接调用。

| API | 事件含义 |
|---|---|
| `send_scan_event(drv, aborted, drv_msg_buf)` | 扫描完成/中止事件；最终促使 supplicant 读取 `wpa_scan_results`。 |
| `mlme_event(drv, bss, drv_msg_buf)` | MLME 总事件分发入口，根据命令调用认证、关联、管理帧等处理器。 |
| `mlme_event_connect(drv, bss, drv_msg_buf)` | 处理 CONNECT/关联成功或失败消息，更新 BSSID、SSID 和状态。 |
| `mlme_event_auth(drv, frame, len)` | 处理 Authentication frame。 |
| `mlme_event_assoc(drv, frame, len)` | 处理 Association Response/关联完成信息。 |
| `mlme_timeout_event(drv, drv_msg_buf)` | 处理认证/关联超时。 |
| `mlme_event_mgmt(bss, drv_msg_buf, frame, len)` | 将收到的管理帧上报到 supplicant/hostapd。 |
| `mlme_event_mgmt_tx_status(drv, drv_msg_buf, frame, len)` | 上报管理帧发送完成、ACK 或失败状态。 |
| `mlme_event_deauth_disassoc(drv, type, frame, len)` | 处理 Deauthentication/Disassociation，更新本地连接状态。 |
| `mlme_event_disconnect(drv, bss, drv_msg_buf)` | 处理 Renesas 驱动断开消息，并转换为 supplicant 断开事件。 |
| `process_drv_event(bss, drv_msg_buf)` | 每个 BSS 的底层事件总入口。该函数会访问 `bss->ctx`/`drv->ctx`，是“不能无上下文调用”的主要原因。 |
| `driver_fc80211_process_global_ev(drv_msg_buf)` | 全局事件入口；无法按 BSS 直接归属的消息从这里开始分发。 |
| `fc80211_wdev_handler(arg)` | wireless device 事件任务回调，负责驱动消息接收/转交。 |

## 8. AP/hostapd 相关补充 API

以下声明在不同配置下才会进入最终镜像，但为了完整性单列说明：

| API | 条件 | 说明 |
|---|---|---|
| `fc80211_set_beacon(ifidx, params)` | 无条件声明，但头文件标记“尚未定义” | 直接设置 Beacon 模板；无 supplicant 设计应使用 `rwnx_cfg80211_start_ap()`/`change_beacon()`，不要依赖未定义入口。 |
| `fc80211_set_beacon_dual(ifidx, params)` | `CONFIG_OWE_TRANS` | OWE Transition 双 Beacon 配置；同样标记为尚未定义。 |
| `fc80211_new_beacon(ifidx, params)` | 无条件声明，但标记尚未定义 | 新建 Beacon。 |
| `fc80211_del_beacon(ifidx)` | 无条件声明，但标记尚未定义 | 删除 Beacon。 |
| `fc80211_join_ibss(ifidx, params, privacy)` | 无条件声明 | 加入 IBSS/ad-hoc 网络；当前 RA6W1 常规 STA/AP 流程通常不使用。 |
| `fc80211_leave_ibss(ifidx)` | 无条件声明 | 离开 IBSS。 |
| `fc80211_new_station(ifidx, params, arg)` | AP 能力 | 增加客户端站点。 |
| `wpa_driver_fc80211_set_ap_isolate(priv, isolate)` | `CONFIG_AP_ISOLATION` | AP 客户端隔离。 |
| `wpa_driver_fc80211_ap(drv, params)` | `CONFIG_AP` | 处理 AP 模式的 associate 参数。 |
| `wpa_driver_fc80211_deinit_ap(priv)` | AP | AP 专用销毁。 |
| `wpa_driver_fc80211_deinit_p2p_cli(priv)` | `CONFIG_P2P` | P2P Client 专用销毁。 |

## 9. 与 RT-Thread WLAN 的映射建议

如果目标是“不经过 supplicant 上下文，直接接入 RT-Thread WLAN”，可以把 demo 的直接入口封装成 `rt_wlan_dev_ops`；推荐的映射如下：

| RT-Thread WLAN | 推荐 Renesas 入口 | 转换重点 |
|---|---|---|
| `wlan_init` | `wifi_low_init` 中的底层初始化序列 | 初始化平台、ROMAC、MAC/driver task、wiphy 和 station interface；不创建 supplicant 上下文。 |
| `wlan_mode` | `rwnx_cfg80211_change_iface()` 或 `fc80211_set_interface()` | `RT_WLAN_STATION/AP` 转换为 `enum fc80211_iftype`；模式切换前先停止当前连接/AP。 |
| `wlan_scan` | `wifi_low_scan` 中的 `rsdev_cfg80211_scan` + `fc80211_get_scan` | 构造与预编译库匹配的 host scan 参数，交由 `rsdev_cfg80211_scan` 转成固件请求，等待底层事件，再把本地扫描结果 ABI 转成 `struct rt_wlan_info`；SSID、BSSID、信道、RSSI、安全类型需要逐字段复制。不要直接把公开 `cfg80211_scan_request` 传给当前库的 `rwnx_cfg80211_scan`。 |
| `wlan_join` | `rwnx_cfg80211_connect()` 或 `fc80211_connect()` | `rt_sta_info` 转 `cfg80211_connect_params`；WPA/WPA2/WPA3 的 cipher、AKM、PMK/SAE 指针和生命周期必须由适配层管理。 |
| `wlan_disconnect` | `rwnx_cfg80211_disconnect()` 或 `fc80211_disconnect()` | 直接提交 reason code，并把底层异步断开事件转换为 RT-Thread 事件。 |
| `wlan_softap` | `rwnx_cfg80211_start_ap()` + `rwnx_cfg80211_change_beacon()` | AP SSID、密码、信道、安全类型、隐藏 SSID 转换为 `cfg80211_ap_settings`/Beacon 数据。 |
| `wlan_ap_stop` | `rwnx_cfg80211_stop_ap()` | 完成后由适配层上报 RT-Thread AP_STOP。 |
| `wlan_get_rssi` | `rwnx_cfg80211_get_station()` 或 `fc80211_get_station()` | 从 `station_info` 提取 RSSI，再转 RT-Thread `int`。 |
| `wlan_get_info` | `fc80211_get_interface_*()` + `rwnx_cfg80211_get_channel()` | 逐字段转换 SSID、BSSID、信道、安全类型和 RSSI。 |
| `wlan_get_mac/wlan_set_mac` | `fc80211_get_interface_macaddr()` + `rwnx_cfg80211_change_iface()` | 读取时复制 6 字节 MAC；设置时通过 `vif_params.macaddr`，不要暴露内部指针。 |
| `wlan_set_powersave` | `rwnx_cfg80211_set_power_mgmt()` 或 `fc80211_set_power_save()` | RT-Thread level 映射成 enable/disable 或 vendor power-save mode。 |
| `wlan_send` | vendor `rwnx_driver_task_send_event()` | RT WLAN 原始以太网帧封装为 vendor `pbuf`，设置 `if_idx` 后提交 TX 事件。 |
| `wlan_recv` | 取决于 vendor RX 回调 | 当前工程 RX 由 Renesas lwIP netif 接管；若要由 RT WLAN 接收，需要增加 RX bridge，不能凭空从 `driver_fc80211.h` 读取数据。 |

### 9.1 不建议直接映射的接口

* 不要把 `struct rt_wlan_device *` 强制转换成 `struct i802_bss *`。
* 不要把 `wpa_driver_fc80211_ops` 直接当作 `rt_wlan_dev_ops`。
* 不要在 `ctx == NULL` 时直接依赖 `process_drv_event()` 的异步事件路径。
* `wpa_driver_fc80211_get_scan_results()` 返回的是 supplicant 的动态结果对象，不是 RT-Thread 的 `rt_wlan_info` 数组；必须逐项复制，并按对应释放函数回收。
* `rsdev_cfg80211_set_pmk()`、`wpa_driver_fc80211_set_key()` 涉及敏感密钥和 WPA 状态机；如果不运行 supplicant，应确认 Renesas `WIFI_ConnectAP()` 是否已经负责这些动作，避免重复下发。

## 10. 独立 demo 的使用方式

编译 `ra6w1-ek` BSP 后，在 RT-Thread shell 中执行：

```text
wifi_low_init
wifi_low_scan
```

`wifi_low_scan` 是全信道通配 SSID 扫描。扫描完成事件来自 `TO_SUPP_QUEUE`，结果通过 `fc80211_get_scan()` 获取；demo 不调用 `driver_fc80211_process_global_ev()`，也不创建 `fc80211_global`、`wpa_driver_fc80211_data` 或 `i802_bss`。

## 11. 独立 demo 的初始化与扫描流程

```text
wifi_low_init()
  -> wifi_hw_platform_prepare()
  -> romac4rtos_initialize()/romac4rtos_config()
  -> rwnx_mac_task_initiailize()/rwnx_driver_task_initiailize()
  -> rwnx_cfg80211_add_iface()

wifi_low_scan()
  -> rsdev_cfg80211_scan()（私有 host 参数 -> cfg80211/固件请求）
  -> TO_SUPP_QUEUE: NEW_SCAN_RESULTS/SCAN_ABORTED
  -> fc80211_get_scan()
  -> 本地扫描结果 ABI
  -> RT-Thread rt_wlan_info
```

下面是原 SDK 服务层的连接流程，仅用于解释头文件中 supplicant 包装 API 的调用关系，独立 demo 不执行这条路径：

```text
WIFI_ConnectAP()
  -> rm_wifi_connect()
  -> supplicant CLI 配置/选择网络
  -> wpa_driver_fc80211_associate()
  -> rsdev_cfg80211_associate()
  -> fc80211_associate()/fc80211_connect()
  -> 固件认证、四次握手和关联
  -> WIFIEvent_t
  -> RT_WLAN_DEV_EVT_CONNECT

WIFI_Disconnect()
  -> rm_wifi_disconnect()
  -> wpa_driver_fc80211_disconnect()
  -> rsdev_cfg80211_disconnect()
  -> fc80211_disconnect()
  -> WIFIEvent_t
  -> RT_WLAN_DEV_EVT_DISCONNECT
```

这里的“无 supplicant 上下文”是指 RT-Thread 适配文件不创建或直接操作 `wpa_supplicant` 对象；Renesas SDK 的内部连接服务仍可能使用其已经编译进库中的 supplicant 状态机，这是保证 WPA/WPA2/WPA3 认证、密钥和异步事件闭环的必要条件。

## 12. 参考源码

* 头文件：`wifi/supplicant/sdk/interfaces/wifi/stack/supplicant/src/drivers/driver_fc80211.h`
* 实现文件：原始工程 `wifi_scan_ek_ra6w1_ep/ra/fsp/lib/rm_wifi/wifiip_a/src/projects/rrq61000/sdk/interfaces/wifi/stack/supplicant/src/drivers/driver_fc80211.c`
* supplicant 通用接口：同目录 `supp_driver.h`
* fc80211 命令、枚举和属性：`wifi/supplicant/sdk/interfaces/wifi/system/include/common/fc80211_copy.h`
* Renesas cfg80211 数据结构：`wifi/supplicant/sdk/interfaces/wifi/system/include/common/rwnx_mac_common.h`

头文件中的函数声明会受 `CONFIG_AP`、`CONFIG_P2P`、`CONFIG_ACS`、`CONFIG_STA_POWER_SAVE`、`CONFIG_AP_POWER`、`IEEE8021X_EAPOL` 等宏影响。阅读或对接时，应以 RT-Thread 工程的最终编译宏和实际链接库为准。
