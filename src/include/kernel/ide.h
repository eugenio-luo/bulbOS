#ifndef _KERNEL_IDE_H
#define _KERNEL_IDE_H

#include <stdint.h>
#include <stddef.h>

#include <kernel/task.h>

typedef struct {
        uintptr_t addr;
        uintptr_t ctrl_addr;
        uintptr_t bus_master_addr;
} ide_channel_t;

typedef struct {
        uint8_t present;
        uint8_t channel;
        uint8_t drive;
        uint8_t type;
        uint16_t signature;
        uint16_t capabilities;
        uint32_t commmand_set;
        uint32_t size;
        uint8_t model[41];
} ide_device_t;

typedef struct {
        uint32_t phys_addr;
        uint16_t byte_count;
        uint16_t reserved;
} __attribute__ ((packed)) prd_entry_t;

typedef struct {
        ide_device_t *device;
        task_info_t  *task;
} disk_request_t;

#define IDE_COMP_PORTS1        0x1F0
#define IDE_COMP_CTRL_PORTS1   0x3F6
#define IDE_COMP_PORTS2        0x170
#define IDE_COMP_CTRL_PORTS2   0x376

#define IDE_PROG_NATMODE1    (1 << 0)
#define IDE_PROG_SWITCH_M1   (1 << 1)
#define IDE_PROG_NATMODE2    (1 << 2)
#define IDE_PROG_SWITCH_M2   (1 << 3)
#define IDE_PROG_DMA         (1 << 7)

#define IDE_ST_BUSY        0x80
#define IDE_ST_READY       0x40
#define IDE_ST_WR_FAULT    0x20
#define IDE_ST_SEEK_COMPL  0x10
#define IDE_ST_DATA_READY  0x8
#define IDE_ST_CORRECTED   0x4
#define IDE_ST_INDEX       0x2
#define IDE_ST_ERROR       0x1

#define IDE_ER_BAD_BLOCK   0x80
#define IDE_ER_UNCOR_DATA  0x40
#define IDE_ER_MD_CH       0x20
#define IDE_ER_ID_NF       0x10
#define IDE_ER_MD_CH_REQ   0x08
#define IDE_ER_ABORTED     0x04
#define IDE_ER_T0_NF       0x02
#define IDE_ER_NO_ADDR_M   0x01

#define IDE_CMD_READ_PIO        0x20
#define IDE_CMD_READ_PIO_EXT    0x24
#define IDE_CMD_READ_DMA        0xC8
#define IDE_CMD_READ_DMA_EXT    0x25
#define IDE_CMD_WRITE_PIO       0x30
#define IDE_CMD_WRITE_PIO_EXT   0x34
#define IDE_CMD_WRITE_DMA       0xCA
#define IDE_CMD_WRITE_DMA_EXT   0x35
#define IDE_CMD_CACHE_FLUSH     0xE7
#define IDE_CMD_CACHE_FLUSH_EXT 0xEA
#define IDE_CMD_PACKET          0xA0
#define IDE_CMD_IDENTIFY_PACKET 0xA1
#define IDE_CMD_IDENTIFY        0xEC

#define IDE_REG_DATA        0x0
#define IDE_REG_ERROR       0x1
#define IDE_REG_FEATURES    0x1
#define IDE_REG_SECCOUNT0   0x2
#define IDE_REG_LBA0        0x3
#define IDE_REG_LBA1        0x4
#define IDE_REG_LBA2        0x5
#define IDE_REG_HDDEVSEL    0x6
#define IDE_REG_COMMAND     0x7
#define IDE_REG_STATUS      0x7
#define IDE_REG_SECCOUNT1   0x8
#define IDE_REG_LBA3        0x9
#define IDE_REG_LBA4        0xA
#define IDE_REG_LBA5        0xB
#define IDE_REG_CONTROL     0xC
#define IDE_REG_ALTSTATUS   0xC
#define IDE_REG_DEVADDRESS  0xD
#define IDE_REG_BUS_COM     0xE
#define IDE_REG_BUS_STAT    0x10
#define IDE_REG_BUS_PRDT    0x12

#define IDE_IDENT_DTYPE         0
#define IDE_IDENT_CYLINDERS     2
#define IDE_IDENT_HEADS         6
#define IDE_IDENT_SECTORS       12
#define IDE_IDENT_SERIAL        20
#define IDE_IDENT_MODEL         54
#define IDE_IDENT_CAPAB         98
#define IDE_IDENT_FVALID        106
#define IDE_IDENT_MAX_LBA       120
#define IDE_IDENT_COMSETS       164
#define IDE_IDENT_MAX_LBA_EXT   200

#define IDE_PRIMARY     0x0
#define IDE_SECONDARY   0x1

#define IDE_ATA         0x0
#define IDE_ATAPI       0x1

#define IDE_MASTER      0x0
#define IDE_SLAVE       0x1

#define SECTOR_SIZE     512

void ide_write_disk(uint8_t device, void *buffer, size_t addr, size_t size);
void ide_read_disk(uint8_t device, size_t addr, size_t size);
void ide_read_disk_buffer(void *buffer, size_t offset, size_t size);
void ide_set(uint32_t bus, uint32_t device, uint32_t func);
void ide_init(void);

#endif
