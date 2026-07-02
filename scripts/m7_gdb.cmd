set confirm off
set pagination off
file build/kernel.elf
target remote localhost:1234
break kmain
break vmm_map_page
break x86_64_trap_dispatch
continue
# Setelah breakpoint tercapai, gunakan:
# info registers cr2 rip rsp
# p/x $cr3
# x/16gx $rsp
# x/8i $rip
