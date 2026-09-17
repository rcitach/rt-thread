# RA6W1 Wi-Fi port

The BSP uses `port/wifi_rt_wlan_adapter.c` as an independent RT-Thread WLAN
station/SoftAP driver. The `wlan0` station and `wlan1` AP modes are bound during application startup;
hardware and firmware initialization then runs in a separate RT-Thread worker,
so a slow firmware boot cannot leave the management layer in `RT_WLAN_NONE`. It does not call `apps/wifi_fc80211_demo.c` or maintain a private
lwIP netif. The standard WLAN device, management and lwIP protocol components
own the interface, link state and DHCP.

## Source ownership

- `port/wifi_rt_wlan_adapter.c`: station/AP initialization, asynchronous
  scan/join, association, SoftAP beacon setup, power saving, and pbuf transport.
- `port/wifi_security_manager.c`: host WPA2-PSK/CCMP key exchange and key installation.
- `port/wifi_wpa_timers.c`: native RT-Thread timer scheduling for the WPA archive.
  The linker redirects its six `eloop_*timeout*` APIs to this implementation.
- `port/wifi_vendor_abi.c`: the event-queue ABI and legacy symbols required by
  the vendor archives. The SDK's legacy `wifi_netif_control` is intercepted so
  it cannot independently alter the WLAN framework's netif.
- `port/rtos_compat.c`: exported ABI symbols for the closed archives, implemented
  using RT-Thread services.
- `port/wifi_crypto_port.c`: CryptoCell/Mbed TLS callbacks and native mutexes.
- `port/wifi_hw_prepare.c`: board clock and Wi-Fi hardware preparation.
- `apps/wifi_net_speed_test.c`: socket throughput test using the active WLAN.
- `crypto/`, `platform/`, `sdk/`, `config/`, `libs/`: vendor sources, configuration,
  headers and prebuilt archives.

The previously removed crypto implementation files have been restored. Their
existing configuration guards and linker section collection determine what
enters the image. `port/wifi_lwip_adapter.c` is also restored, but explicitly
excluded from compilation together with `apps/wifi_fc80211_demo.c`.

New port code uses RT-Thread queues, mutexes, threads, timers and allocators.
The prebuilt MAC/driver/supplicant archives still call the FreeRTOS ABI, so the
FreeRTOS_Wrapper package remains a binary dependency. `StaticQueue_t` supplies
only its public handle layout; the event queue itself is a native `rt_mq` and
the worker consumes it with `rt_mq_recv`. This is not a claim that the closed
archives have been rebuilt for native RT-Thread.

Port diagnostics use `LOG_E`, `LOG_W` and `LOG_I`, with tags such as `wifi.wlan`,
`wifi.sec`, `wifi.crypto`, `wifi.hw` and `wifi.os`.

## Data path and lifetime

`RT_WLAN_PROT_LWIP_ENABLE` and `RT_WLAN_PROT_LWIP_PBUF_FORCE` enable the WLAN
framework's existing pbuf adapter. RX transfers the driver's pbuf reference
straight to that protocol; a rejected packet is freed by the driver adapter.
The vendor RX ABI uses `VIF + 2`; it is normalized before WLAN delivery.
TX shares a contiguous, exclusively owned `PBUF_RAM` by adding one reference
for the firmware. Chained, custom, shared or insufficient-headroom buffers are
copied into a suitable TX pbuf. A failed queue operation releases only the
reference acquired for that operation. The caller's reference remains valid.

The archive expects a 16-byte pbuf, with its interface index at byte 15 and TX
descriptors immediately after the structure. Compile-time checks enforce this
ABI, 456 bytes of headroom and 36 bytes of tailroom. `LWIP_NETIF_TX_SINGLE_PBUF`
is retained to reduce normal TCP chaining; it alone does not make arbitrary
pbufs safe for this firmware.

`LWIP_NO_TX_THREAD` removes an extra synchronous mailbox hop. The WLAN pbuf
adapter propagates driver failures, and the direct Ethernet output path maps
resource exhaustion to lwIP `ERR_MEM`, so socket retry logic can see it.

One blocking event worker serializes driver events and EAPOL handling. Native
timer callbacks only wake this worker; WPA callbacks execute under the same
control mutex as join/disconnect. WLAN notifications run outside that mutex
to avoid lock inversion with management callbacks. Scan snapshots are bounded
to 64 entries and replaced on each scan. The firmware BSS cache remains valid
for association and is released before the next scan or when disabling WLAN. Keys and WPA contexts are released
on disconnection. After an irreversible vendor-task startup failure, live
queue/thread resources are retained and initialization is not repeated; the
vendor API provides no safe task shutdown operation. Reboot after such a
failure. A timed-out scan is not reused until its abort completion arrives.

The station and SoftAP paths support open networks and WPA2-PSK with CCMP.
Station passphrases of 8–63 characters and 64 hexadecimal PSKs are accepted;
SoftAP passwords must be 8–63 characters. WPA3, enterprise authentication,
mandatory PMF and passive-only scans are not implemented. Unsupported
authentication is rejected, rather than reported as an open network. The
country table uses the BSP's `COUNTRY_CODE_DEFAULT`; power saving defaults to
off. The AP uses the second hardware VIF (`wlan1`) and a 2.4 GHz 20 MHz
channel. Configure a static AP address or enable RT-Thread's DHCP server for
clients that need automatic IPv4 configuration.

## Build and run

From the BSP directory:

```sh
RTT_EXEC_PATH=/home/rain/.tools/toolchain/arm-gnu-14.3.rel1-none-eabi/bin scons -j4
```

After flashing, use the standard WLAN commands. The station is registered and
attached to the WLAN lwIP protocol at startup:

```text
msh /> wifi scan
msh /> wifi join xiaomi <password>
msh /> wifi ap ra6w1-ap <password>
msh /> wifi status
msh /> ifconfig
msh /> ifconfig w1 192.168.10.1 255.255.255.0
msh /> ping 10.252.47.144
msh /> wifi_speed_test udp_tx 10.252.47.144 5002 10
msh /> wifi_speed_test udp_rx 10.252.47.144 5002 10
msh /> wifi disc
msh /> wifi ap_stop
```

Wait for the `wifi.wlan` ready message and the WLAN `Got IP address` message before socket tests. Old `wifi_low_*`
and `wifi_wlan_test` commands are no longer built. Applications can use
`rt_wlan_connect`, `rt_wlan_disconnect`, WLAN event handlers and
`rt_wlan_dev_set_powersave` directly.

The speed command reports `rx_drv` received/dropped frames and `tx_wlan`
accepted frames, queue/event rejections, fallback copies and minimum free TX
slots. TX acceptance is not an over-the-air delivery measurement. Compare it
with the remote receiver's throughput/loss statistics. Real association,
DHCP, disconnect/reconnect and throughput still require board validation.
