#ifndef MCSOS_ARCH_IDT_H
#define MCSOS_ARCH_IDT_H
#include <stdint.h>

#define IDT_ENTRIES 48u

typedef struct __attribute__((packed)) {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
    uint64_t vector, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} trap_frame_t;

void idt_init(void);
void x86_64_trap_dispatch(trap_frame_t *f);

#endif
