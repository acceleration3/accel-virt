asm(R"(
    .text
    .globl __chkstk
    .p2align 4
__chkstk:
    mov r10, rsp
    add r10, 8
    mov r11, r10
    sub r11, rax
    cmp r11, r10
    jbe av_chkstk_page
    xor r11d, r11d
av_chkstk_page:
    sub r10, 0x1000
    cmp r10, r11
    jb av_chkstk_last
    test byte ptr [r10], 0
    jmp av_chkstk_page
av_chkstk_last:
    test byte ptr [r11], 0
    ret
)");
