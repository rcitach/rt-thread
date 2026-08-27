# RA6W1 Wi-Fi port

This directory contains the Wi-Fi middleware port for the RA6W1-EK BSP.

## Directory layout

- `app/`: RT-Thread startup entry. It opens MAP/VEE and starts `WIFI_On()`.
- `config/`: Wi-Fi, lwIP, mbedTLS, VEE and FreeRTOS compatibility settings.
- `driver/`: vendor archives. The four Wi-Fi archives are `libmacsw.a`,
  `librwnx_drv.a`, `libromaclib.a` and `libsupplicant.a`. The CC312 property
  archive is also linked because the original project requires it.
- `platform/`: Renesas Wi-Fi FSP adapters, RT-Thread ABI adapters, GPIO,
  flash, VEE, MAP, watchdog, UART and the selected Wi-Fi/lwIP glue.
- `protocol/lwip/`: vendor lwIP declarations and reference sources. Its lwIP
  core is not compiled by this BSP.
- `crypto/`: mbedTLS source and configuration.
- `supplicant/`: Wi-Fi SDK headers and source used by the port.
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
MAP instance first; MAP opens the VEE/NVRAM service, which opens the block
media and OSPI flash instance. It then calls `WIFI_On()`.

`WIFI_On()` starts the Wi-Fi driver but does not select an SSID or start an
automatic connection. Use the public Wi-Fi API after startup, or change the
application to call `WIFI_OnAuto()` when valid connection data is already
stored in NVRAM.

## RT-Thread lwIP boundary

The protocol stack is RT-Thread's `components/net/lwip/lwip-2.1.2`. Its
core, `sys_arch`, memory management and timeout scheduler are compiled by
RT-Thread. The Wi-Fi port only compiles the selected adapter and network
management files listed in `wifi/SConscript`.

The vendor DHCP Server, SNTP and network-management APIs are retained where
they are part of the Wi-Fi feature. Vendor-private lwIP extensions are
replaced by `rtthread_lwip_compat.c`; in particular, the DHCP Server PCB is
kept in the server module instead of adding a field to RT-Thread's `struct
netif`. The original SDK still uses the FreeRTOS ABI for Wi-Fi driver and
supplicant code, so `platform/rtos_compat.c` and the FreeRTOS wrapper remain
required. This does not make the network protocol stack a FreeRTOS lwIP
stack. No source file under RT-Thread's `components/net/lwip` is required to
be modified: Wi-Fi-specific interface-index and interface-name compatibility
is kept under this `wifi/` directory and in the BSP's generated `rtconfig.h`.

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
