#include <stdint.h>
#include <stdio.h>

#include <kernel/ps2controller.h>

static uint8_t
ps2_getresponse(void)
{
        while (!(inb(PS2_STATUSREG) & 0x1));
        
        uint8_t response = inb(PS2_DATAPORT);
        return response;
}

static void
ps2_sendnextbyte(uint8_t byte)
{
        while ((inb(PS2_STATUSREG) & 0x2));

        outb(byte, PS2_DATAPORT);
}

static void
ps2_sendcommand(uint8_t command, uint8_t expected_res)
{
        outb(command, PS2_COMMANDREG);
        uint8_t ret = ps2_getresponse();
        if (ret != expected_res) {
                kprintf("PS2 controller: expected response: %x, actual: %x\n",
                        expected_res, ret);
        }
}

static void
ps2_flushbuffer(void)
{
        while ((inb(PS2_STATUSREG) & 0x1))
                inb(PS2_DATAPORT);
}

static void
ps2_setconfigbyte(void)
{
        outb(PS2_CMD_READ_BYTE + 0, PS2_COMMANDREG);
        uint8_t config_byte = ps2_getresponse();

        config_byte |= PS2_REG_INT1;
        config_byte &= ~PS2_REG_TRANSLAT; 
        
        if (!(config_byte & (1 << 5))) {
                kprintf("it isn't a dual channel controller\n");
        }
        
        outb(PS2_CMD_WRITE_BYTE + 0, PS2_COMMANDREG);
        ps2_sendnextbyte(config_byte);
}

void
ps2_init(void)
{
        outb(PS2_CMD_DISABLE1, PS2_COMMANDREG);
        outb(PS2_CMD_DISABLE2, PS2_COMMANDREG);
        ps2_flushbuffer();

        ps2_sendcommand(PS2_CMD_TEST_CTRL, 0x55);
        ps2_setconfigbyte();

        ps2_sendcommand(PS2_CMD_TEST1, 0);
        ps2_sendcommand(PS2_CMD_TEST2, 0);
       
        outb(PS2_CMD_ENABLE1, PS2_COMMANDREG);
        outb(PS2_CMD_ENABLE2, PS2_COMMANDREG);
       
        kprintf("PS2 setup SUCCESS\n");
}
