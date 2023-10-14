#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <kernel/serialport.h>

static inline void serial_write_reg(uintptr_t port, serial_reg_t reg, uint8_t data);

static inline uint8_t
serial_read_reg(uintptr_t port, serial_reg_t reg)
{
        /* to read the BAUD rate register, it's needed to have the DLAB bit set,
           to save the line control register's content, the register is first
           read */
        uint8_t line_control_reg;
        if (reg >= SERIAL_REG_BAUD_LEAST) {
                line_control_reg = serial_read_reg(port, SERIAL_REG_LINE_CTRL);

                serial_write_reg(port, SERIAL_REG_LINE_CTRL,
                                 line_control_reg | SERIAL_DLAB);

                reg -= SERIAL_REG_BAUD_LEAST;
        }
        
        uint8_t data = inb(port + reg);

        if (reg >= SERIAL_REG_BAUD_LEAST)
                serial_write_reg(port, SERIAL_REG_LINE_CTRL,
                                 line_control_reg & ~SERIAL_DLAB);

        return data;
}

static inline void
serial_write_reg(uintptr_t port, serial_reg_t reg, uint8_t data)
{
        /* to write the BAUD rate register, it's needed to have the DLAB bit set,
           to save the line control register's content, the register is first
           read */
        uint8_t line_control_reg;
        if (reg >= SERIAL_REG_BAUD_LEAST) {
                line_control_reg = serial_read_reg(port, SERIAL_REG_LINE_CTRL);

                serial_write_reg(port, SERIAL_REG_LINE_CTRL,
                                 line_control_reg | SERIAL_DLAB);

                reg -= SERIAL_REG_BAUD_LEAST;
        }
        
        outb(data, port + reg);

        if (reg >= SERIAL_REG_BAUD_LEAST)
                serial_write_reg(port, SERIAL_REG_LINE_CTRL,
                                 line_control_reg & ~SERIAL_DLAB);
}

uint32_t
serial_initialize(void)
{
        kprintf("[SERIALPORT] setup STARTING\n");

        /* disable all interrupt */
        serial_write_reg(SERIAL_COM1, SERIAL_REG_INT, SERIAL_NO_INT);

        /* setting baud rate at 3 */
        serial_write_reg(SERIAL_COM1, SERIAL_REG_BAUD_LEAST, 3);
        serial_write_reg(SERIAL_COM1, SERIAL_REG_BAUD_MOST, 0);

        /* 8 bits, no parity, one stop bit */
        uint8_t data = SERIAL_8_BIT | SERIAL_1_STOP | SERIAL_NO_PAR; 
        serial_write_reg(SERIAL_COM1, SERIAL_REG_LINE_CTRL, data);

        /* enable FIFO with 14 bytes threshold */
        data = SERIAL_ENABLE_FIFO | SERIAL_CL_TRANSMIT | SERIAL_CL_RECEIVE;
        data |= SERIAL_14_BYTES_INT;
        serial_write_reg(SERIAL_COM1, SERIAL_REG_FIFO_CTRL, data);

        /* loopback mode to test */
        data = SERIAL_LOOP | SERIAL_OUT2 | SERIAL_OUT1 | SERIAL_RTS;
        serial_write_reg(SERIAL_COM1, SERIAL_REG_MOD_CTRL, data);

        /* test */
        serial_write_reg(SERIAL_COM1, SERIAL_REG_DATA, 0xAE);

        if (serial_read_reg(SERIAL_COM1, SERIAL_REG_DATA) != 0xAE)
                return 1;

        /* back to normal mode, without loopback */
        data = SERIAL_OUT1 | SERIAL_OUT2 | SERIAL_DTR | SERIAL_RTS;
        serial_write_reg(SERIAL_COM1, SERIAL_REG_MOD_CTRL, data);
        
        kprintf("[SERIALPORT] setup COMPLETE\n");
        return 0;
}

static bool
serial_received(void)
{
        uint8_t data = serial_read_reg(SERIAL_COM1, SERIAL_REG_LINE_STAT);
        return data & SERIAL_DATA_READY;
}

uint8_t
serial_readb(void)
{
        while (!serial_received());

        return serial_read_reg(SERIAL_COM1, SERIAL_REG_DATA);
}

static bool
serial_isempty(void)
{
        uint8_t data = serial_read_reg(SERIAL_COM1, SERIAL_REG_LINE_STAT);
        return data & SERIAL_BUFFER_EMPTY;
}

void
serial_writeb(uint8_t byte)
{
        while (!serial_isempty());

        serial_write_reg(SERIAL_COM1, SERIAL_REG_DATA, byte);
}

void
serial_write(const char *data, size_t size)
{
        const uint8_t *entry = (uint8_t*) data;
        const uint8_t *last = entry + size;

        for (; entry < last; ++entry) {
                serial_writeb(*entry);

                if (*entry == '\n')
                        serial_writeb('\r');
        }
}

void
serial_writestring(const char *data)
{
        serial_write(data, strlen(data));
}

void
serial_read(char *data, size_t size)
{
        char *entry = data;
        char *last = entry + size;

        for (; entry < last; ++entry)
                *entry = serial_readb();
}
