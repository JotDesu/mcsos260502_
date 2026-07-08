.RECIPEPREFIX := >
SHELL := /usr/bin/env bash
BUILD_DIR := build
KERNEL := $(BUILD_DIR)/kernel.elf
MAP := $(BUILD_DIR)/kernel.map
DISASM := $(BUILD_DIR)/kernel.disasm.txt
SYMS := $(BUILD_DIR)/kernel.syms.txt
ISO := $(BUILD_DIR)/mcsos.iso
ISO_ROOT := iso_root
LIMINE_DIR := third_party/limine
CC := clang
HOSTCC := clang
AS := clang
LD := ld.lld
OBJDUMP := objdump
READELF := readelf
NM := nm
COMMON_CFLAGS := --target=x86_64-unknown-none-elf -std=c17 -ffreestanding \
  -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie \
  -fno-lto -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mno-mmx \
  -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror \
  -Ikernel/arch/x86_64/include -Ikernel/include -Iinclude
CFLAGS := $(COMMON_CFLAGS)
ASFLAGS := --target=x86_64-unknown-none-elf -m64 -march=x86-64 \
  -fno-pic -fno-pie -mcmodel=kernel
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T linker.ld

SRC_C := $(shell find kernel -name '*.c' -not -path 'kernel/tests/*' | LC_ALL=C sort)
SRC_S := $(shell find kernel -name '*.S' -not -path 'kernel/tests/*' | LC_ALL=C sort)
OBJ   := $(patsubst %.c, $(BUILD_DIR)/normal/%.o, $(SRC_C)) \
         $(patsubst %.S, $(BUILD_DIR)/normal/%.o, $(SRC_S))

.PHONY: all build inspect image clean distclean grade check-m6 check-m7

all: build inspect

build: $(KERNEL)

$(BUILD_DIR)/normal/%.o: %.c
>mkdir -p $(dir $@)
>$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/normal/%.o: %.S
>mkdir -p $(dir $@)
>$(AS) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJ) linker.ld
>mkdir -p $(BUILD_DIR)
>$(LD) $(LDFLAGS) -Map=$(MAP) -o $@ $(OBJ)

inspect: $(KERNEL)
>$(READELF) -h $(KERNEL) > $(BUILD_DIR)/kernel.readelf.header.txt
>$(READELF) -l $(KERNEL) > $(BUILD_DIR)/kernel.readelf.programs.txt
>$(NM) -n $(KERNEL) > $(SYMS)
>$(OBJDUMP) -d -Mintel $(KERNEL) > $(DISASM)
>grep -q 'ELF64' $(BUILD_DIR)/kernel.readelf.header.txt
>grep -q 'Machine:[[:space:]]*Advanced Micro Devices X86-64' $(BUILD_DIR)/kernel.readelf.header.txt
>grep -q 'kmain' $(SYMS)
>grep -q 'kernel_panic_at' $(SYMS)
>grep -q 'cpu_halt_forever' $(DISASM)
>grep -q 'idt_init' $(SYMS)
>grep -q 'pic_remap' $(SYMS)
>grep -q 'pit_configure_hz' $(SYMS)
>grep -q 'isr_stub_32' $(SYMS)
>grep -q 'timer_on_irq0' $(SYMS)
>grep -q 'pmm_init_from_map' $(SYMS)
>grep -q 'pmm_alloc_frame' $(SYMS)

grade: build inspect
>@echo "M6 static grade: PASS"

check-m6: $(BUILD_DIR)/pmm.o $(BUILD_DIR)/test_pmm_host
>./$(BUILD_DIR)/test_pmm_host
>$(NM) -u $(BUILD_DIR)/pmm.o | tee $(BUILD_DIR)/pmm.undefined.txt
>test ! -s $(BUILD_DIR)/pmm.undefined.txt
>$(OBJDUMP) -dr $(BUILD_DIR)/pmm.o > $(BUILD_DIR)/pmm.objdump.txt
>@echo "[PASS] M6 static check selesai"

$(BUILD_DIR)/pmm.o: kernel/core/pmm.c kernel/include/mcsos/kernel/pmm.h
>mkdir -p $(BUILD_DIR)
>$(CC) $(CFLAGS) -c kernel/core/pmm.c -o $(BUILD_DIR)/pmm.o

$(BUILD_DIR)/test_pmm_host: kernel/core/pmm.c kernel/tests/test_pmm_host.c kernel/include/mcsos/kernel/pmm.h
>mkdir -p $(BUILD_DIR)
>$(HOSTCC) -std=c17 -Wall -Wextra -Werror -Ikernel/include \
>  kernel/core/pmm.c kernel/tests/test_pmm_host.c -o $(BUILD_DIR)/test_pmm_host

image: $(KERNEL)
>mkdir -p $(ISO_ROOT)/boot/limine $(ISO_ROOT)/EFI/BOOT
>cp -v $(KERNEL) $(ISO_ROOT)/boot/kernel.elf
>cp -v configs/limine/limine.conf $(ISO_ROOT)/boot/limine/
>cp -v $(LIMINE_DIR)/limine-bios.sys $(ISO_ROOT)/boot/limine/
>cp -v $(LIMINE_DIR)/limine-bios-cd.bin $(ISO_ROOT)/boot/limine/
>cp -v $(LIMINE_DIR)/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine/
>cp -v $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
>xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
>  -no-emul-boot -boot-load-size 4 -boot-info-table \
>  --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part \
>  --efi-boot-image --protective-msdos-label $(ISO_ROOT) -o $(ISO)
>$(LIMINE_DIR)/limine bios-install $(ISO)
>sha256sum $(ISO) > $(BUILD_DIR)/mcsos.iso.sha256

clean:
>rm -rf $(BUILD_DIR)

distclean: clean
>rm -rf iso_root limine

.PHONY: check-m7

check-m7: $(BUILD_DIR)/vmm.o $(BUILD_DIR)/test_vmm_host
>./$(BUILD_DIR)/test_vmm_host
>$(NM) -u $(BUILD_DIR)/vmm.o | tee $(BUILD_DIR)/vmm.undefined.txt
>test ! -s $(BUILD_DIR)/vmm.undefined.txt
>$(OBJDUMP) -dr $(BUILD_DIR)/vmm.o > $(BUILD_DIR)/vmm.objdump.txt
>grep -q "invlpg" $(BUILD_DIR)/vmm.objdump.txt
>grep -q "cr3" $(BUILD_DIR)/vmm.objdump.txt
>@echo "[PASS] M7 static check selesai"

$(BUILD_DIR)/vmm.o: kernel/core/vmm.c kernel/include/mcsos/kernel/vmm.h
>mkdir -p $(BUILD_DIR)
>$(CC) $(CFLAGS) -c kernel/core/vmm.c -o $(BUILD_DIR)/vmm.o

$(BUILD_DIR)/test_vmm_host: kernel/core/vmm.c kernel/tests/test_vmm_host.c kernel/include/mcsos/kernel/vmm.h
>mkdir -p $(BUILD_DIR)
>$(HOSTCC) -std=c17 -Wall -Wextra -Werror -Ikernel/include -DMCSOS_HOST_TEST \
>  kernel/core/vmm.c kernel/tests/test_vmm_host.c -o $(BUILD_DIR)/test_vmm_host

.PHONY: check-m8

check-m8: $(BUILD_DIR)/kmem.o $(BUILD_DIR)/test_kmem_host
>./$(BUILD_DIR)/test_kmem_host | tee $(BUILD_DIR)/test_kmem.log
>grep -q 'PASS' $(BUILD_DIR)/test_kmem.log
>$(NM) -u $(BUILD_DIR)/kmem.o | tee $(BUILD_DIR)/kmem.undefined.txt
>test ! -s $(BUILD_DIR)/kmem.undefined.txt
>$(READELF) -h $(BUILD_DIR)/kmem.o > $(BUILD_DIR)/kmem.readelf.header.txt
>$(OBJDUMP) -dr $(BUILD_DIR)/kmem.o > $(BUILD_DIR)/kmem.objdump.txt
>@echo "[PASS] M8 static check selesai"

$(BUILD_DIR)/kmem.o: kernel/mm/kmem.c include/mcsos/kmem.h
>mkdir -p $(BUILD_DIR)
>$(CC) $(CFLAGS) -Iinclude -c kernel/mm/kmem.c -o $(BUILD_DIR)/kmem.o

$(BUILD_DIR)/test_kmem_host: kernel/mm/kmem.c tests/test_kmem.c include/mcsos/kmem.h
>mkdir -p $(BUILD_DIR)
>$(HOSTCC) -std=c17 -Wall -Wextra -Werror -Iinclude \
>  kernel/mm/kmem.c tests/test_kmem.c -o $(BUILD_DIR)/test_kmem_host

.PHONY: check-m9

check-m9: $(BUILD_DIR)/sched_combined.o $(BUILD_DIR)/test_sched_host
>./$(BUILD_DIR)/test_sched_host | tee $(BUILD_DIR)/test_sched.log
>grep -q 'PASS' $(BUILD_DIR)/test_sched.log
>$(NM) -u $(BUILD_DIR)/sched_combined.o | tee $(BUILD_DIR)/sched.undefined.txt
>test ! -s $(BUILD_DIR)/sched.undefined.txt
>$(OBJDUMP) -dr $(BUILD_DIR)/sched_combined.o > $(BUILD_DIR)/sched.objdump.txt
>grep -q 'mcsos_context_switch' $(BUILD_DIR)/sched.objdump.txt
>@echo "[PASS] M9 static check selesai"

$(BUILD_DIR)/sched.o: kernel/core/sched.c kernel/include/mcsos/kernel/sched.h
>mkdir -p $(BUILD_DIR)
>$(CC) $(CFLAGS) -c kernel/core/sched.c -o $(BUILD_DIR)/sched.o

$(BUILD_DIR)/context_switch.o: kernel/arch/x86_64/src/context_switch.S
>mkdir -p $(BUILD_DIR)
>$(AS) $(ASFLAGS) -c kernel/arch/x86_64/src/context_switch.S -o $(BUILD_DIR)/context_switch.o

$(BUILD_DIR)/sched_combined.o: $(BUILD_DIR)/sched.o $(BUILD_DIR)/context_switch.o
>$(LD) -r -o $(BUILD_DIR)/sched_combined.o $(BUILD_DIR)/sched.o $(BUILD_DIR)/context_switch.o

$(BUILD_DIR)/test_sched_host: kernel/core/sched.c kernel/tests/test_sched_host.c kernel/include/mcsos/kernel/sched.h
>mkdir -p $(BUILD_DIR)
>$(HOSTCC) -std=c17 -Wall -Wextra -Werror -Ikernel/include -DMCSOS_HOST_TEST \
>  kernel/core/sched.c kernel/tests/test_sched_host.c -o $(BUILD_DIR)/test_sched_host

.PHONY: check-m10

check-m10: $(BUILD_DIR)/syscall_combined.o $(BUILD_DIR)/test_syscall_host
>./$(BUILD_DIR)/test_syscall_host | tee $(BUILD_DIR)/test_syscall.log
>grep -q 'M10 syscall host tests passed' $(BUILD_DIR)/test_syscall.log
>$(NM) -u $(BUILD_DIR)/syscall_combined.o | tee $(BUILD_DIR)/syscall.undefined.txt
>test ! -s $(BUILD_DIR)/syscall.undefined.txt
>$(READELF) -h $(BUILD_DIR)/syscall_combined.o > $(BUILD_DIR)/syscall.readelf.header.txt
>grep -q 'Machine:[[:space:]]*Advanced Micro Devices X86-64' $(BUILD_DIR)/syscall.readelf.header.txt
>$(OBJDUMP) -dr $(BUILD_DIR)/syscall_combined.o > $(BUILD_DIR)/syscall.objdump.txt
>grep -q 'x86_64_syscall_int80_stub' $(BUILD_DIR)/syscall.objdump.txt
>grep -q 'iretq' $(BUILD_DIR)/syscall.objdump.txt
>@echo "[PASS] M10 static check selesai"

$(BUILD_DIR)/syscall.o: kernel/syscall/syscall.c include/mcsos/syscall.h
>mkdir -p $(BUILD_DIR)
>$(CC) $(CFLAGS) -c kernel/syscall/syscall.c -o $(BUILD_DIR)/syscall.o

$(BUILD_DIR)/syscall_entry.o: kernel/arch/x86_64/src/syscall_entry.S
>mkdir -p $(BUILD_DIR)
>$(AS) $(ASFLAGS) -c kernel/arch/x86_64/src/syscall_entry.S -o $(BUILD_DIR)/syscall_entry.o

$(BUILD_DIR)/syscall_combined.o: $(BUILD_DIR)/syscall.o $(BUILD_DIR)/syscall_entry.o
>$(LD) -r -o $(BUILD_DIR)/syscall_combined.o $(BUILD_DIR)/syscall.o $(BUILD_DIR)/syscall_entry.o

$(BUILD_DIR)/test_syscall_host: kernel/syscall/syscall.c tests/test_syscall_host.c include/mcsos/syscall.h
>mkdir -p $(BUILD_DIR)
>$(HOSTCC) -std=c17 -Wall -Wextra -Werror -Iinclude \
>  kernel/syscall/syscall.c tests/test_syscall_host.c -o $(BUILD_DIR)/test_syscall_host
