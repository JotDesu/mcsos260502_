#ifndef MCSOS_KERNEL_SERIAL_H
#define MCSOS_KERNEL_SERIAL_H

#ifdef __cplusplus
extern "C" {
#endif

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *s);

#ifdef __cplusplus
}
#endif

#endif
