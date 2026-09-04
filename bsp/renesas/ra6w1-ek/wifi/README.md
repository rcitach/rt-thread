# RA6W1 Wi-Fi port

This directory contains the Wi-Fi middleware port for the RA6W1-EK BSP.

## Directory layout

- `app/`: minimal RT-Thread hardware test entry. It starts the Wi-Fi MAC and
  driver tasks, reads the OTP MAC, and provides a low-level scan command.
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

## Standalone cfg80211 demo

The current application entry is `app/wifi_fc80211_demo.c`. It does not create
a `wpa_supplicant` context and does not call `WIFI_*` or `rm_wifi_*` service
APIs. It performs the platform/ROMAC setup, starts the MAC and driver tasks,
creates a station interface, sends a scan through `rsdev_cfg80211_scan()`,
receives the asynchronous completion event, and reads the BSS records.
Before using public connect/disconnect operations it also explicitly creates
the vendor cfg80211 response queue with `rwnx_cfg80211_initialize()`; the
original supplicant startup normally performs this step indirectly.

After flashing, run either the new names or their compatibility aliases:

```text
msh /> wifi_low_init
msh /> wifi_low_scan
```

The aliases `wifi_demo_init` and `wifi_demo_scan` are also exported for
existing scripts. `wifi_low_scan` enumerates enabled channels from both the
driver's 2.4 GHz and 5 GHz band tables, passes an explicit frequency list, and
prints BSSID, frequency, RSSI and SSID. The shipped `librwnx_drv.a` has a
private host-side scan-parameter ABI behind `rsdev_cfg80211_scan()`; the demo
keeps a local ABI-matched adapter and never passes a public
`struct cfg80211_scan_request` directly to `rwnx_cfg80211_scan()`.

### Connection test

The same file exports a direct cfg80211 connection test. The command builds a
`struct cfg80211_connect_params` locally and calls the lower-level
`rwnx_cfg80211_connect()` entry point directly. The request is sent to the
driver task and the command waits for the asynchronous `FC80211_CMD_CONNECT`
completion event. Use an open network to test the link without any key
material:

```text
msh /> wifi_low_connect_open <ssid>
msh /> wifi_low_status
msh /> wifi_low_disconnect
```

For WPA2-Personal, pass either the normal 8..63 byte password or a raw
32-byte PSK/PMK as exactly 64 hexadecimal characters:

```text
msh /> wifi_low_connect <ssid> <password|psk_hex64>
msh /> wifi_low_status
msh /> wifi_low_disconnect
```

When scanning is intentionally omitted, provide the AP's exact center
frequency and BSSID so the firmware does not have to select an AP from
`bssid=NULL, freq=0`:

```text
msh /> wifi_low_connect <ssid> <password|psk_hex64> <freq_mhz> <bssid>
msh /> wifi_low_connect xiaomi password 2437 12:34:56:78:9A:BC
```

The open-network command accepts the same optional target pair. The original
SDK normally fills these two values from the selected scan result before
converting the request through `rsdev_cfg80211_associate()`. This demo passes
them directly in `cfg80211_connect_params`, so the conversion layer is not
part of the test.

The demo derives a password with WPA's PBKDF2-HMAC-SHA1 procedure locally and
keeps the resulting 32-byte PMK in the association context. The first
password-form request registers the Mbed TLS/CryptoCell platform callbacks and
executes the original `R_CC312_Crypto_Init()` sequence; a raw 64-digit PSK
bypasses this derivation step. It does not create a supplicant context or run
the WPA four-way handshake, so a successful low-level association alone is not
the same as a fully authenticated WPA2 data link. An unsuccessful request
prints the IEEE status code or a 15-second completion timeout.

## cfg80211 API probe

`app/wifi_cfg_api_test.c` exports `wifi_cfg_api_test`. It references every
external declaration in `rwnx_cfg.h` so a missing archive symbol is detected at
link time, calls only low-risk operations with the initialized idle station,
and reports connection/AP/management-frame/event callbacks as `SKIP` with the
reason they need real state. Use:

```text
msh /> wifi_cfg_api_test
msh /> wifi_cfg_api_test scan
```

The `scan` argument runs the asynchronous scan adapter as an additional test.
The probe intentionally does not fabricate connect, AP, key, management-frame
or firmware-event payloads; those tests must be added around a real connection
or AP state to avoid changing RF state or dereferencing an invalid ABI.

The older `app/wifi_hw_test.c` remains in the tree as a diagnostic reference,
but it is not the application selected by `wifi/SConscript` and it creates the
supplicant driver's `global -> driver -> bss` context.

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

## Debugging the prebuilt Wi-Fi archives

The MAC implementation is in `driver/libmacsw.a`, so source-level stepping
inside `rwnx_mac_task_initiailize()` is not available. The final ELF still has
function symbols and complete machine code. Always use addresses from the
current `rtthread.elf`/`rtthread.map`; addresses from the original demo image
are different after the link layout changes.

The current startup path is:

```text
umac_lmac_init
  -> rwnx_mac_task_initiailize
     -> four xQueueGenericCreate calls
     -> rwnx_platform_on
        -> rwnx_hw_power_up
           -> rwnx_hw_power_up_step0..step4
```

For a J-Link GDB server listening on port 2331, load the current ELF and set
breakpoints before continuing. Stop any other GDB client first because the
J-Link GDB server accepts one debugging connection:

```gdb
set pagination off
target extended-remote :2331
monitor reset
monitor halt
hbreak rwnx_mac_task_initiailize
hbreak rwnx_platform_on
hbreak __assert_func
hbreak BusFault_Handler
hbreak rt_hw_hard_fault_exception
hbreak Default_WiFi_Handler
continue
```

Add `rwnx_hw_power_up_step1` or an instruction-address breakpoint only after
the first pass, and remove an unused breakpoint when the probe reports that
hardware breakpoint slots are exhausted. `MemManage_Handler` and
`UsageFault_Handler` can be added in the same way when the fault registers
indicate those exception classes.

At the MAC breakpoint, disassemble the function and step over each queue
creation. If all four queue handles are non-zero, the next relevant call is
`rwnx_platform_on`; a lack of a return from that call means the failure is in
the hardware power-up path, not in queue creation:

```gdb
disassemble rwnx_mac_task_initiailize
info registers r0 r1 r2 r3 r4 r5 sp lr pc xpsr
x/5wx 0x2002629c
```

The diagnostic print in the RT-Thread queue wrapper is emitted before
`rt_malloc()` and `rt_mq_create()`. Four printed lines therefore only prove
that four calls entered the wrapper. In the current link, the return points
after the four calls are `0x41b8c`, `0x41b9a`, `0x41ba8`, and `0x41bb8`; use
the fourth return point, or break in `rt_mq_create`, to distinguish an
allocator problem from the later platform power-up call. The instruction at
`0x41bc2` is the call to `rwnx_platform_on` after the queue checks.

For the current link, `rwnx_hw_power_up_step1` reads
`CRG_TOP->SYS_STATUS_REG` at `0x400c0034` and later reads the Wi-Fi AHB
window at `0x609000e0`. Bit 3 is `MAC_IS_UP` and bit 5 is `PHY_IS_UP`.
Set a breakpoint on the literal load and single-step the following memory
access:

```gdb
hbreak *0x47a88
hbreak *0x47b1a
continue
# At 0x47a88: `si`; inspect the SYS_STATUS_REG read at 0x400c0034.
# At 0x47b1a: this executes the read from the Wi-Fi AHB window at 0x609000e0.
info registers pc lr sp r0 r1 r2 r3 xpsr
```

The hard-coded addresses above are valid only for the current build. Obtain
them after every link with:

```sh
TOOL=/home/rain/.tools/toolchain/arm-gnu-14.3.rel1-none-eabi/bin
"$TOOL/arm-none-eabi-nm" -n rtthread.elf | grep -E \
  'rwnx_mac_task_initiailize|rwnx_platform_on|rwnx_hw_power_up_step1'
"$TOOL/arm-none-eabi-objdump" -d rtthread.elf \
  --start-address=$("$TOOL/arm-none-eabi-nm" -n rtthread.elf | \
  awk '$3 == "rwnx_hw_power_up_step1" {print "0x"$1}') \
  --stop-address=0x78000
```

When GDB stops in `__assert_func`, inspect its arguments immediately. The
power-up timeout passes the source file, line number, function and expression
in the normal ARM calling registers:

```gdb
monitor halt
x/s $r0
p/d $r1
x/s $r2
x/s $r3
```

When GDB stops in `rt_hw_hard_fault_exception`, this BSP's Cortex-M33 entry
code passes a pointer to an RT-Thread exception context in `r0`. The current
build uses hard-float and the TrustZone-aware context layout is: `exc_return`
at offset 0, `tz/lr/psplim/control` at offsets 4..16, saved `r4-r11` at
offsets 20..48, and the hardware core frame at offset 52. Therefore the
stacked PC is at `r0+76`, stacked R2 is at `r0+60`, stacked LR is at `r0+72`,
and stacked PSR is at `r0+80`:

```gdb
monitor halt
set $ctx = $r0
p/x *(unsigned int *)($ctx + 76)  # stacked PC
p/x *(unsigned int *)($ctx + 60)  # stacked R2
p/x *(unsigned int *)($ctx + 72)  # stacked LR
p/x *(unsigned int *)($ctx + 80)  # stacked PSR
x/21wx $ctx
x/i *(unsigned int *)($ctx + 76)
p/x *(unsigned int *)0xE000ED28  # CFSR
p/x *(unsigned int *)0xE000ED2C  # HFSR
p/x *(unsigned int *)0xE000ED34  # MMFAR
p/x *(unsigned int *)0xE000ED38  # BFAR
```

`BusFault_Handler` in this BSP saves the fault frame and then loops without
calling the RT-Thread hard-fault hook. At a breakpoint on the first
instruction of `BusFault_Handler`, read the original frame before stepping
over the prologue. For a basic (non-FPU) frame:

```gdb
set $fault_sp = $msp
if (($lr & 4) != 0)
  set $fault_sp = $psp
end
x/8wx $fault_sp
p/x *(unsigned int *)($fault_sp + 24)  # fault PC
p/x *(unsigned int *)($fault_sp + 8)   # fault R2
p/x *(unsigned int *)($fault_sp + 20)  # fault LR
x/i *(unsigned int *)($fault_sp + 24)
```

If `($lr & 0x10) == 0`, the processor allocated an extended FPU frame; keep
the raw dump and account for that frame before applying the basic-frame
offsets.

Check bit 4 of the saved `exc_return` before interpreting the dump. If it is
clear, the assembly entry also saves `d8-d15`; inspect the raw stack and
account for that extra 64-byte area before using the core-frame offsets. The
typed expression below is useful for the normal non-FPU exception case:

```gdb
p/x ((struct exception_info *)$r0)->stack_frame.exception_stack_frame.pc
p/x ((struct exception_info *)$r0)->stack_frame.exception_stack_frame.r2
```

Reading registers while the target is still running can produce sentinel
values such as `0xDEADBEEF`; the same value is also used to initialize unused
RT-Thread stack words. It is not a valid fault context until the target is
halted and the stacked frame has been read.

Interpret the result as follows:

- A stop in `__assert_func`, or a timeout branch near `rwnx_hw_power_up_step1`,
  means `MAC_IS_UP` or `PHY_IS_UP` did not become set. Check
  `0x400c0034`, the sleep controls at `0x400c0028`, the PHY clock gate in
  `0x400c0000`, and the radio reset writes made by `step0`/`step1`.
- A BusFault with stacked PC at the `ldr.w` that reads `0x609000e0` means the
  Wi-Fi AHB window was not accessible at that time. The literal value
  `0x60900000` is expected; its presence in the disassembly does not prove
  that the peripheral is powered or clocked.
- A stop in `Default_WiFi_Handler` means an unexpected or unimplemented Wi-Fi
  interrupt vector was taken.
- A non-zero queue handle followed by a stop before `xTaskCreate` confirms
  that the failure is inside `rwnx_platform_on`/power-up. A normal queue or
  task allocation failure returns to `umac_lmac_init` and prints `Fail`.

The archive can also be inspected without source code. This reveals object
names, symbols and relocations, although it cannot restore C statements:

```sh
TOOL=/home/rain/.tools/toolchain/arm-gnu-14.3.rel1-none-eabi/bin
mkdir -p /tmp/libmacsw_extract
cd /tmp/libmacsw_extract
ar x /path/to/wifi/driver/libmacsw.a rwnx_mac_task.c.obj rwnx_hw.c.obj
"$TOOL/arm-none-eabi-readelf" -S -s -r rwnx_mac_task.c.obj
"$TOOL/arm-none-eabi-objdump" -d -r rwnx_mac_task.c.obj
```

`addr2line` will show `??:?` for these objects when the vendor archive has no
DWARF information. Use the final ELF for symbols and the map file for the
archive member that supplied each function. In this port, `machw_mib` must
also be forced into the link after CLI sources are removed; `wifi/SConscript`
contains the `-u,machw_mib` link anchor for that purpose.
