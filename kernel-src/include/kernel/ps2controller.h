#ifndef _KERNEL_PS2CONTROLLER_H
#define _KERNEL_PS2CONTROLLER_H

#include <stdint.h>

#define PS2_CMD_READ_BYTE     0x20
#define PS2_CMD_WRITE_BYTE    0x60
#define PS2_CMD_DISABLE2      0xA7
#define PS2_CMD_ENABLE2       0xA8
#define PS2_CMD_TEST2         0xA9
#define PS2_CMD_TEST_CTRL     0xAA
#define PS2_CMD_TEST1         0xAB
#define PS2_CMD_DISABLE1      0xAD
#define PS2_CMD_ENABLE1       0xAE
#define PS2_CMD_DIS_SCAN      0xF5

#define PS2_REG_INT1          (1 << 0)
#define PS2_REG_TRANSLAT      (1 << 6)

#define PS2_DATAPORT          0x60
#define PS2_STATUSREG         0x64
#define PS2_COMMANDREG        0x64

void ps2_init(void);

#endif
