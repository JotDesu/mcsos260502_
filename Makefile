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
AS := clang
LD := ld.lld
OBJDUMP := objdump
READELF := readelf
NM := nm
COMMON_CFLAGS := --target=x86_64-unknown-none-elf -std=c17 -ffreestanding \
  -fno-builtin -fno-stack-protector -fno-stack-check -fno-pic -fno-pie \
  -fno-lto -m64 -march=x86-64 -mabi=sysv -mno-red-zone -mno-mmx \
  -mno-sse -mno-sse2 -mcmodel=kernel -Wall -Wextra -Werror \
  -Ikernel/arch/x86_64/include -Ikernel/include
CFLAGS := $(COMMON_CFLAGS)
ASFLAGS := --target=x86_64-unknown-none-elf -m64 -march=x86-64 \
  -fno-pic -fno-pie -mcmodel=kernel
LDFLAGS := -nostdlib -static -z max-page-size=0x1000 -T linker.ld

SRC_C := $(shell find kernel -name '*.c' | LC_ALL=C sort)
SRC_S := $(shell find kernel -name '*.S' | LC_ALL=C sort)
OBJ   := $(patsubst %.c, $(BUILD_DIR)/normal/%.o, $(SRC_C)) \
         $(patsubst %.S, $(BUILD_DIR)/normal/%.o, $(SRC_S))

.PHONY: all build inspect image clean distclean grade

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

grade: build inspect
>@echo "M5 static grade: PASS"

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
