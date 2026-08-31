# Wi-Fi backup demo

`apps/wifi_fc80211_demo.c` is the standalone cfg80211 demo used by the
RA6W1-EK BSP. It is selected by the BSP's top-level `SConscript` and is linked
with the archives in `libs/`. The old `../wifi/SConscript` is retained only
for legacy callers.

## Ownership

This directory is the active Wi-Fi source tree for this BSP. The Wi-Fi build
script, application, port layer, configuration, SDK adapters, crypto adapters
and vendor archives are all kept below `wifi_back/`.
The default top-level BSP build skips the old `wifi/` directory; its
`SConscript` is retained only for legacy direct callers and is not an active
source or include root.

- `include/`: local public headers for the hardware preparation and crypto
  port layers.
- `platform/`: clock preparation, ROM veneers, RTOS compatibility,
  FSP adapters, lwIP/DHCP glue and the Wi-Fi platform glue.
- `sdk/`: `rwnx_cfg.h`, cfg80211 ABI structures, PTIM headers,
  FreeRTOS/driver headers and the supplicant compatibility headers.
- `crypto/` and `platform/r_cc312_openable_w/`: Mbed TLS and
  CryptoCell objects used for WPA2 PSK derivation.
- `config/`, `os/` and the RT-Thread lwIP component: configuration,
  allocator, timing and network stack support.
- `port/wifi_crypto_port.c`: CryptoCell callback bridge used by the
  password-form connection command.

The five files under `libs/` are the vendor archive inputs for this backup
demo: `libmacsw.a`, `librwnx_drv.a`, `libromaclib.a`, `libsupplicant.a` and
`libr_cc312_property_w.a`.

## Build

Run from the BSP directory:

```sh
RTT_EXEC_PATH=/home/rain/.tools/toolchain/arm-gnu-14.3.rel1-none-eabi/bin \
scons -j4
```

The build must produce `rtthread.elf` and include symbols from
`wifi_back/apps/wifi_fc80211_demo.c` and `wifi_back/libs/librwnx_drv.a`.

## Run

After flashing the image:

```text
msh /> wifi_low_init
msh /> wifi_low_scan
msh /> wifi_low_connect xiaomi 12345678 2437 02:5C:AD:40:32:C7
msh /> wifi_low_status
msh /> wifi_low_disconnect
```

The connection command calls `rwnx_cfg80211_connect()` directly. A successful
`FC80211_CMD_CONNECT` event confirms the low-level association path. The demo
does not start the complete WPA Supplicant state machine or DHCP workflow.
