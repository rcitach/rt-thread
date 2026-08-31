# RA6W1 Wi-Fi port

This directory contains the Wi-Fi middleware port for the RA6W1-EK BSP.

## Directory layout

- `app/`: minimal RT-Thread startup entry. It starts `WIFI_On()`, prints the
  MAC address, selects station mode, and performs one scan.
- `config/`: Wi-Fi, lwIP, mbedTLS, VEE and FreeRTOS compatibility settings.
- `driver/`: vendor archives. The four Wi-Fi archives are `libmacsw.a`,
  `librwnx_drv.a`, `libromaclib.a` and `libsupplicant.a`. The CC312 property
  archive is also linked because the original project requires it.
- `platform/`: Renesas Wi-Fi FSP adapters, RT-Thread ABI adapters, GPIO,
  flash, VEE, MAP, watchdog, UART and the selected Wi-Fi/lwIP glue.
- `protocol/lwip/`: vendor lwIP declarations kept for source compatibility.
  Its lwIP core is not compiled by this BSP.
- `crypto/`: only the mbedTLS, PSA and CC312 objects required by the WPA
  archive are compiled; unused crypto sources are omitted.
- `supplicant/`: Wi-Fi SDK headers plus the small PTIM/WPA adapter sources
  used by the port. The full supplicant implementation remains in the
  prebuilt archive.
- `os/`: small OS compatibility headers.

## Build

Run from the BSP directory. `RTT_EXEC_PATH` is set explicitly because the
checked-in `rtconfig.py` default is a Windows toolchain path:

```sh
RTT_EXEC_PATH=/home/rain/.tools/toolchain/arm-gnu-14.3.rel1-none-eabi/bin \
scons -j4
```

The build produces `rtthread.elf`, `rtthread.bin`, `rtthread.hex` and the
RA6W1 flash image.

## Startup sequence

`app/wifi_app.c` starts a delayed RT-Thread worker. The worker opens the
Watchdog Service, calls `WIFI_On()`, reads the MAC through `WIFI_GetMAC()`,
selects station mode, disconnects any previous connection, and calls
`WIFI_Scan()` once. The Wi-Fi middleware opens VEE/NVRAM on demand. The
worker then remains alive so the Wi-Fi middleware tasks continue to run.

`WIFI_On()` starts the Wi-Fi driver but does not select an SSID or start an
automatic connection. Use the public Wi-Fi API after startup, or change the
application to call `WIFI_OnAuto()` when valid connection data is already
stored in NVRAM.

## RT-Thread lwIP boundary

The protocol stack is RT-Thread's `components/net/lwip/lwip-2.1.2`. Its
core, `sys_arch`, memory management and timeout scheduler are compiled by
RT-Thread. The Wi-Fi port only compiles the selected adapter and network
management files listed in `wifi/SConscript`.

Only the Wi-Fi adapter, network-interface glue, and the DHCP server helper
needed by the selected build are compiled. Vendor SNTP, HTTP, WebSocket,
ping, lwiperf, OTA and product application modules are not compiled. Vendor-
private lwIP compatibility is kept under this `wifi/` directory, including
the DHCP-server PCB storage; no field is added to RT-Thread's `struct netif`.
The original SDK still uses the FreeRTOS ABI for the Wi-Fi driver and
supplicant archive, so `platform/rtos_compat.c` and the FreeRTOS wrapper
remain required. This does not make the network protocol stack a FreeRTOS
lwIP stack. No source file under RT-Thread's `components/net/lwip` is
modified.

## Porting boundary

- `platform/rtos_compat.c` exports the FreeRTOS ABI symbols referenced by the
  prebuilt archives and maps them to RT-Thread memory, timer and interrupt
  services.
- The DA1640 clock-manager implementation is excluded for RA6W1. The port
  keeps the middleware-visible clock state while FSP owns the actual MCU
  clock configuration.
- `platform/rom/rom_api_veneers.S` provides Thumb veneers for the ROM
  functions called by the prebuilt Wi-Fi archives. The corresponding entries
  are removed from `script/rom_code_gcc.symbols` so the linker can type these
  calls as functions without absolute-symbol branch warnings.
- `script/rom_code_gcc.symbols` is required by `libromaclib.a` and the Wi-Fi
  SDK. The ROMAC/DPM/PTIM symbols are fixed-address ROM interfaces, so the
  image must run with the matching Wi-Fi/ROM hardware and firmware.
- EVK WPS/factory-reset GPIO application code is not included in the startup
  port; its callback is intentionally a no-op. It can be added later under
  `app/` together with an RT-Thread external-interrupt adapter.
