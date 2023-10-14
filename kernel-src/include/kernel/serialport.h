#ifndef _KERNEL_SERIALPORT_H
#define _KERNEL_SERIALPORT_H

#include <stddef.h>
#include <stdint.h>

#define SERIAL_COM1   0x3F8

typedef enum {
        SERIAL_REG_DATA = 0,
        SERIAL_REG_INT,
        SERIAL_REG_FIFO_CTRL,
        SERIAL_REG_LINE_CTRL,
        SERIAL_REG_MOD_CTRL,
        SERIAL_REG_LINE_STAT,
        SERIAL_REG_MOD_STAT,
        SERIAL_REG_SCRATCH, 
        SERIAL_REG_BAUD_LEAST,
        SERIAL_REG_BAUD_MOST,
} serial_reg_t;

/* SERIAL_REG_INT */
#define SERIAL_NO_INT   0x0
#define SERIAL_INT      0x1

/* SERIAL_REG_FIFO_CTRL */
#define SERIAL_ENABLE_FIFO   (0x1 << 0)
#define SERIAL_CL_RECEIVE    (0x1 << 1)
#define SERIAL_CL_TRANSMIT   (0x1 << 2)

#define SERIAL_14_BYTES_INT  (0x3 << 6)

/* SERIAL_REG_LINE_CTRL */
#define SERIAL_5_BIT        0x0
#define SERIAL_6_BIT        0x1
#define SERIAL_7_BIT        0x2
#define SERIAL_8_BIT        0x3

#define SERIAL_1_STOP      (0x0 << 2)
#define SERIAL_2_STOP      (0x1 << 2)

#define SERIAL_NO_PAR      (0x0 << 3)
#define SERIAL_ODD_PAR     (0x1 << 3)
#define SERIAL_EVEN_PAR    (0x3 << 3)
#define SERIAL_MARK_PAR    (0x5 << 3)
#define SERIAL_SPACE_PAR   (0x7 << 3)

#define SERIAL_DLAB        (0x1 << 7)

/* SERIAL_REG_MOD_CTRL */
#define SERIAL_DTR         (0x1 << 0)
#define SERIAL_RTS         (0x1 << 1)
#define SERIAL_OUT1        (0x1 << 2)
#define SERIAL_OUT2        (0x1 << 3)
#define SERIAL_LOOP        (0x1 << 4)

/* SERIAL_REG_LINE_STAT */
#define SERIAL_DATA_READY     (0x1 << 0)
#define SERIAL_BUFFER_EMPTY   (0x1 << 5)

uint32_t serial_initialize(void);
uint8_t serial_readb(void);
void serial_writeb(uint8_t byte);
void serial_write(const char *data, size_t size);
void serial_writestring(const char *data);
void serial_read(char *data, size_t size);

#endif
