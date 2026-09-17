/* __chkstk for binaries with no C runtime: the compiler calls it before a
 * frame larger than a page is used, with the frame's size in rax, so the frame
 * is touched a page at a time from the top and each guard page is met in
 * order. Without it a frame skips the guard page and faults - rvkglprobe.exe's
 * entry did, with a 45 KiB frame over a 4 KiB commit - and an ICD would do the
 * same on an application's thread.
 *
 * The routine is the Windows x64 contract: probe from the caller's stack
 * pointer (rsp past the return address) down by rax, preserve every register
 * but r10, r11 and the flags, and leave rsp for the caller to move. It is
 * written in assembly because it runs before its caller's frame exists. */
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
