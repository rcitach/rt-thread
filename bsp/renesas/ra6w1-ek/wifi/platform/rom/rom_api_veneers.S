/* Thumb veneers for ROM functions referenced by the prebuilt Wi-Fi code. */

.syntax unified
.cpu cortex-m33
.thumb

.section .text.rom_api_veneers, "ax", %progbits
.align 2

.macro ROM_VENEER name, address
    .global \name
    .type \name, %function
    .thumb_func
\name:
    ldr r12, =\address
    bx r12
    .size \name, .-\name
.endm

ROM_VENEER CC_BsvLcsGet,          0x0f0415a1
ROM_VENEER da16x_crc8ccitt,        0x0f0469ad
ROM_VENEER dpm_get_rtclk,          0x0f047769
ROM_VENEER dpmtm_add_twt_timer,    0x0f0492c9
ROM_VENEER mm_sec_machwkey_rec,    0x0f04bd79
ROM_VENEER mm_sec_machwkey_store,  0x0f04be29
ROM_VENEER romac_init_primitives,  0x0f04c439
ROM_VENEER rtos_setup_feat,        0x0f04cb15
ROM_VENEER rtos_setup_func,        0x0f04cc3d
ROM_VENEER timp_set_backup_addr,   0x0f0537c9

.ltorg

.section .note.GNU-stack, "", %progbits
