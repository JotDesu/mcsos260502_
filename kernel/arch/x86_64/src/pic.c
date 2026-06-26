#include <mcsos/arch/pic.h>
#include <mcsos/arch/io.h>

#define PIC_MASTER_CMD  0x20u
#define PIC_MASTER_DATA 0x21u
#define PIC_SLAVE_CMD   0xA0u
#define PIC_SLAVE_DATA  0xA1u

#define ICW1_INIT 0x11u
#define ICW4_8086 0x01u
#define PIC_EOI   0x20u

void pic_remap(uint8_t master_offset, uint8_t slave_offset) {
    uint8_t master_mask = inb(PIC_MASTER_DATA);
    uint8_t slave_mask  = inb(PIC_SLAVE_DATA);

    outb(PIC_MASTER_CMD,  ICW1_INIT);  io_wait();
    outb(PIC_SLAVE_CMD,   ICW1_INIT);  io_wait();
    outb(PIC_MASTER_DATA, master_offset); io_wait();
    outb(PIC_SLAVE_DATA,  slave_offset);  io_wait();
    outb(PIC_MASTER_DATA, 0x04u); io_wait();
    outb(PIC_SLAVE_DATA,  0x02u); io_wait();
    outb(PIC_MASTER_DATA, ICW4_8086); io_wait();
    outb(PIC_SLAVE_DATA,  ICW4_8086); io_wait();

    outb(PIC_MASTER_DATA, master_mask);
    outb(PIC_SLAVE_DATA,  slave_mask);
}

void pic_mask_all(void) {
    outb(PIC_MASTER_DATA, 0xFFu);
    outb(PIC_SLAVE_DATA,  0xFFu);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    if (irq < 8u) {
        port  = PIC_MASTER_DATA;
        value = inb(port) & (uint8_t)(~(1u << irq));
    } else {
        port  = PIC_SLAVE_DATA;
        value = inb(port) & (uint8_t)(~(1u << (irq - 8u)));
    }
    outb(port, value);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8u) {
        outb(PIC_SLAVE_CMD, PIC_EOI);
    }
    outb(PIC_MASTER_CMD, PIC_EOI);
}

uint8_t pic_read_master_mask(void) { return inb(PIC_MASTER_DATA); }
uint8_t pic_read_slave_mask(void)  { return inb(PIC_SLAVE_DATA);  }
