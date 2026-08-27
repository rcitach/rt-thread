#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BSP_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

ELF="${RA6W1_ELF:-${BSP_DIR}/rtthread.elf}"
IMAGE="${RA6W1_IMAGE:-${BSP_DIR}/rtthread.img.bin}"
ARM_GDB="${ARM_GDB:-/home/rain/.tools/toolchain/arm-gnu-14.3.rel1-none-eabi/bin/arm-none-eabi-gdb}"
JLINK_EXE="${JLINK_EXE:-/usr/bin/JLinkExe}"
JLINK_GDB_SERVER="${JLINK_GDB_SERVER:-/usr/bin/JLinkGDBServer}"
JLINK_DEVICE="${JLINK_DEVICE:-R7SA6W1CE}"
JLINK_INTERFACE="${JLINK_INTERFACE:-SWD}"
JLINK_SPEED="${JLINK_SPEED:-4000}"
JLINK_FLASH_SPEED="${JLINK_FLASH_SPEED:-1000}"
FLASH_ADDRESS="${RA6W1_FLASH_ADDRESS:-0x0A000000}"
GDB_PORT="${GDB_PORT:-2331}"
SWO_PORT="${SWO_PORT:-2332}"
TELNET_PORT="${TELNET_PORT:-2333}"

server_args=(
    -device "${JLINK_DEVICE}"
    -if "${JLINK_INTERFACE}"
    -speed "${JLINK_SPEED}"
    -port "${GDB_PORT}"
    -swoport "${SWO_PORT}"
    -telnetport "${TELNET_PORT}"
    -nogui
    -singlerun
)

usage()
{
    printf 'Usage: %s server|gdb|flash\n' "$0"
    printf '\n'
    printf '  server  Start J-Link GDB Server on TCP port %s\n' "${GDB_PORT}"
    printf '  gdb     Start the server and attach arm-none-eabi-gdb to %s\n' "${ELF}"
    printf '  flash   Program %s at %s\n' "${IMAGE}" "${FLASH_ADDRESS}"
}

start_server()
{
    exec "${JLINK_GDB_SERVER}" "${server_args[@]}"
}

start_gdb()
{
    local server_pid
    local gdb_status

    if [[ ! -f "${ELF}" ]]; then
        printf 'ELF not found: %s\n' "${ELF}" >&2
        exit 1
    fi
    if [[ ! -x "${ARM_GDB}" ]]; then
        printf 'ARM GDB not executable: %s\n' "${ARM_GDB}" >&2
        exit 1
    fi

    "${JLINK_GDB_SERVER}" "${server_args[@]}" &
    server_pid=$!
    trap 'kill "${server_pid}" 2>/dev/null || true' EXIT INT TERM

    sleep 1
    if ! kill -0 "${server_pid}" 2>/dev/null; then
        wait "${server_pid}" || true
        printf 'J-Link GDB Server stopped before GDB connected\n' >&2
        exit 1
    fi

    if "${ARM_GDB}" "${ELF}" \
        -ex 'set pagination off' \
        -ex 'set confirm off' \
        -ex "target extended-remote 127.0.0.1:${GDB_PORT}" \
        -ex 'monitor reset' \
        -ex 'monitor halt' \
        -ex 'break main'; then
        gdb_status=0
    else
        gdb_status=$?
    fi

    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
    trap - EXIT INT TERM
    return "${gdb_status}"
}

flash_image()
{
    if [[ ! -f "${IMAGE}" ]]; then
        printf 'Image not found: %s\n' "${IMAGE}" >&2
        exit 1
    fi
    if [[ ! -x "${JLINK_EXE}" ]]; then
        printf 'J-Link Commander not executable: %s\n' "${JLINK_EXE}" >&2
        exit 1
    fi

    "${JLINK_EXE}" \
        -device "${JLINK_DEVICE}" \
        -if "${JLINK_INTERFACE}" \
        -speed "${JLINK_FLASH_SPEED}" \
        -autoconnect 1 <<JLINK_COMMANDS
loadfile ${IMAGE} ${FLASH_ADDRESS}
qc
JLINK_COMMANDS
}

case "${1:-}" in
    server)
        start_server
        ;;
    gdb)
        start_gdb
        ;;
    flash)
        flash_image
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
