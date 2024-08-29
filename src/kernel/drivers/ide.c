#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/ide.h>
#include <kernel/pci.h>
#include <kernel/memory.h>
#include <kernel/apic.h>
#include <kernel/isrs.h>
#include <kernel/idt.h>
#include <kernel/vmm.h>
#include <kernel/task.h>

ide_channel_t channels[2];
ide_device_t ide_devices[4];
prd_entry_t *prd_table;
disk_request_t actual_disk_req;

struct {
        uint8_t buffer[KIB(1)];
} __attribute__ ((packed)) *ide_mem_buffer;

static uint8_t is_ide_set = 0;
static uint8_t ide_buffer[1024] = {0};

void
ide_set(uint32_t bus, uint32_t device, uint32_t func)
{
        uint32_t prog_if = (pci_read_reg(bus, device, func, 2 << 2) >> 8) & 0xFF;
        uintptr_t bar0 = pci_read_reg(bus, device, func, 4 << 2);
        uintptr_t bar1 = pci_read_reg(bus, device, func, 5 << 2);
        uintptr_t bar2 = pci_read_reg(bus, device, func, 6 << 2);
        uintptr_t bar3 = pci_read_reg(bus, device, func, 7 << 2);
        uintptr_t bar4 = pci_read_reg(bus, device, func, 8 << 2);

        bar4 = (bar4 & 1) ? bar4 & 0xFFFC : bar4 & 0xFFF0; 

        channels[0].addr = (prog_if & IDE_PROG_NATMODE1) ? bar0 : IDE_COMP_PORTS1;
        channels[0].ctrl_addr = (prog_if & IDE_PROG_NATMODE1) ? bar1 : IDE_COMP_CTRL_PORTS1;
        channels[0].bus_master_addr = (prog_if & IDE_PROG_DMA) ? bar4 : 0;

        channels[1].addr = (prog_if & IDE_PROG_NATMODE2) ? bar2 : IDE_COMP_PORTS2;
        channels[1].ctrl_addr = (prog_if & IDE_PROG_NATMODE2) ? bar3 : IDE_COMP_CTRL_PORTS2;
        channels[1].bus_master_addr = (prog_if & IDE_PROG_DMA) ? (bar4 + 8) : 0;

        is_ide_set = 1;
}

static void
ide_write(uint32_t channel, uint32_t reg, uint8_t data)
{
        uintptr_t port;

        /* bus master registers */
        if (reg >= IDE_REG_BUS_COM) {

                reg -= 0xE;
                port = channels[channel].bus_master_addr;

        /* normal registers */
        } else if (reg <= IDE_REG_LBA5) {

                if (reg >= IDE_REG_SECCOUNT1) reg -= 0x6;
                port = channels[channel].addr;

        /* control register / altstatus register */
        } else {

                reg -= 0xC;
                port = channels[channel].ctrl_addr;
        }
        
        outb(data, port + reg);
}

static uint8_t
ide_read(uint32_t channel, uint32_t reg)
{
        uintptr_t port;

        /* bus master registers */
        if (reg >= IDE_REG_BUS_COM) {

                reg -= 0xE;
                port = channels[channel].bus_master_addr;

        /* normal registers */
        } else if (reg <= IDE_REG_LBA5) {

                if (reg >= IDE_REG_SECCOUNT1) reg -= 0x6;
                port = channels[channel].addr;

        /* control register / altstatus register */
        } else {

                reg -= 0xC;
                port = channels[channel].ctrl_addr;
        }
        
        return inb(port + reg);
}

static void
ide_read_buffer(uint32_t channel, uint32_t reg, void *buffer, uint32_t count)
{
        uintptr_t port;

        /* bus master registers */
        if (reg >= IDE_REG_BUS_COM) {

                reg -= 0xE;
                port = channels[channel].bus_master_addr;

        /* normal registers */
        } else if (reg <= IDE_REG_LBA5) {

                if (reg >= IDE_REG_SECCOUNT1) reg -= 0x6;
                port = channels[channel].addr;

        /* control register / altstatus register */
        } else {

                reg -= 0xC;
                port = channels[channel].ctrl_addr;
        }
        
        insl(port + reg, buffer, count);
}

static void
ide_detect_drive(uint32_t channel, uint32_t drive)
{
        uint32_t index = channel * 2 + drive;
        ide_devices[index].present = 0;

        ide_write(channel, IDE_REG_HDDEVSEL, 0xA0 | (drive << 4));
        sleep(1);

        ide_write(channel, IDE_REG_COMMAND, IDE_CMD_IDENTIFY);
        sleep(1);

        if (!ide_read(channel, IDE_REG_STATUS)) return;
        
        uint8_t error = 0;
        while (1) {
                uint8_t status = ide_read(channel, IDE_REG_STATUS);
                if (status & IDE_ST_ERROR) {
                        error = 1;
                        break;
                }

                if (!(status & IDE_ST_BUSY) && (status & IDE_ST_DATA_READY))
                        break;
        }

        uint8_t type = IDE_ATA;
        if (error) {
                uint8_t cl = ide_read(channel, IDE_REG_LBA1);
                uint8_t ch = ide_read(channel, IDE_REG_LBA2);

                if ((cl == 0x14 && ch == 0xEB) || (cl == 0x69 && ch == 0x96))
                        type = IDE_ATAPI;
                else
                        return;

                ide_write(channel, IDE_REG_COMMAND, IDE_CMD_IDENTIFY_PACKET);
                sleep(1);
        }
        
        ide_read_buffer(channel, IDE_REG_DATA, &ide_buffer[0], 256);
        
        ide_devices[index] = (ide_device_t) {
                .present = 1,
                .type = type,
                .channel = channel,
                .drive = drive,
                .signature = *(uint16_t*)&ide_buffer[IDE_IDENT_DTYPE],
                .capabilities = *(uint16_t*)&ide_buffer[IDE_IDENT_CAPAB],
                .commmand_set = *(uint32_t*)&ide_buffer[IDE_IDENT_COMSETS],
        };

        ide_write(channel, IDE_REG_COMMAND, IDE_CMD_IDENTIFY);

        uint8_t addressing_type = ide_devices[index].commmand_set & (1 << 26);
        uint32_t info_index = (addressing_type) ? IDE_IDENT_MAX_LBA_EXT : IDE_IDENT_MAX_LBA;
        ide_devices[index].size = *(uint32_t*)&ide_buffer[info_index];

        for (uint32_t i = 0; i < 40; i += 2) {
                ide_devices[index].model[i] = ide_buffer[IDE_IDENT_MODEL + i + 1];
                ide_devices[index].model[i + 1] = ide_buffer[IDE_IDENT_MODEL + i];
        }
        ide_devices[index].model[40] = 0;
}

static inline void
ide_set_prdt(uint32_t channel, uintptr_t prdt_addr)
{
        outl(prdt_addr, channels[channel].bus_master_addr + 0x4);
}

static void
ide_set_prd_entry(uint32_t prdt_entry, size_t size)
{
        prd_table[prdt_entry].phys_addr = VIR2PHY((uintptr_t)&ide_mem_buffer[prdt_entry]);
        prd_table[prdt_entry].byte_count = (size == KIB(64)) ? 0 : size;
        prd_table[prdt_entry].reserved = 0;
}

static void
ide_fill_prd_table(ide_device_t *device, uint8_t *buffer, size_t size, int is_write)
{
        ide_set_prdt(device->channel, VIR2PHY((uintptr_t)prd_table));

        uint32_t count = 0, start = 0;
        while (size - start) {
                size_t prd_size = (size > KIB(1)) ? KIB(1) : size;
                
                ide_set_prd_entry(count, prd_size);
                if (is_write) memcpy(ide_mem_buffer[count].buffer, &buffer[start], prd_size);

                ++count;
                start += prd_size;
        }
        prd_table[count - 1].reserved |= (1 << 15); 
       
        ide_write(device->channel, IDE_REG_BUS_COM, (is_write) ? 1 << 3 : 0);
        ide_write(device->channel, IDE_REG_BUS_COM, 1);       
}

void
ide_read_disk_buffer(void *buffer, size_t offset, size_t size)
{
        memcpy(buffer, (uint8_t*)(ide_mem_buffer->buffer) + offset, size);      
}

static void
ide_set_device(ide_device_t *device, size_t lba, size_t sectors)
{
        uint8_t head = (lba > 0x10000000) ? 0 : (lba >> 24) & 0xF;

        ide_write(device->channel, IDE_REG_HDDEVSEL, 0xE0 | (device->drive << 4) | head);
        sleep(1);
        
        ide_write(device->channel, IDE_REG_FEATURES, 0);
        while (~ide_read(device->channel, IDE_REG_STATUS) & (1 << 6));

        ide_write(device->channel, IDE_REG_SECCOUNT1, 0);
        if (lba > 0x10000000) {
                ide_write(device->channel, IDE_REG_LBA3, (lba >> 24) && 0xFF);
        } else {
                ide_write(device->channel, IDE_REG_LBA3, 0);
        }
        ide_write(device->channel, IDE_REG_LBA4, 0);
        ide_write(device->channel, IDE_REG_LBA5, 0);
        
        ide_write(device->channel, IDE_REG_SECCOUNT0, sectors);
        ide_write(device->channel, IDE_REG_LBA0, lba & 0xFF);
        ide_write(device->channel, IDE_REG_LBA1, (lba >> 8) & 0xFF);
        ide_write(device->channel, IDE_REG_LBA2, (lba >> 16) & 0xFF);
}

static void
ide_access_disk(ide_device_t *device, uint8_t *buffer, size_t lba,
                size_t size, int is_write)
{
        if (!size)
                return;

        actual_disk_req = (disk_request_t) {
                .device = device,
                .task   = current_task,
        };

        ide_fill_prd_table(device, buffer, size, is_write);

        while (~ide_read(device->channel, IDE_REG_STATUS) & (1 << 6));
        ide_set_device(device, lba, (size - 1) / 512 + 1);

        ide_write(device->channel, IDE_REG_BUS_STAT, 1);
        ide_write(device->channel, IDE_REG_CONTROL, 0);

        uint8_t command;
        if (is_write) {
                command = (lba > 0x10000000) ?
                        IDE_CMD_WRITE_DMA_EXT :
                        IDE_CMD_WRITE_DMA; 
        } else {
                command = (lba > 0x10000000) ?
                        IDE_CMD_READ_DMA_EXT :
                        IDE_CMD_READ_DMA; 
        }

        ide_write(device->channel, IDE_REG_COMMAND, command);

        task_block(IO_REQUEST);
}

void
ide_write_disk(uint8_t device_idx, void *buffer, size_t addr, size_t size)
{
        ide_access_disk(ide_devices + device_idx, (uint8_t*)buffer, addr / 512, size, 1);
}        

void
ide_read_disk(uint8_t device_idx, size_t addr, size_t size)
{
        ide_access_disk(ide_devices + device_idx, NULL, addr / 512, size, 0);
}

__attribute__ ((interrupt))
static void
ide_handler(interrupt_frame_t *frame)
{
        (void) frame;
        
        ide_device_t *device = actual_disk_req.device;

        uint32_t bus_stat = ide_read(device->channel, IDE_REG_BUS_STAT);
        uint32_t reg_stat = ide_read(device->channel, IDE_REG_STATUS);
        ide_write(device->channel, IDE_REG_BUS_STAT, (1 << 2));

        if (~bus_stat & (1 << 2)) {
                lapic_sendEOI();
                return;
        }

        if (reg_stat & 1) {
                kprintf("[IDE] error in data transfer\n");
        }
        
        ide_write(device->channel, IDE_REG_BUS_COM, 0);
        task_unblock(actual_disk_req.task);

        lapic_sendEOI();
}

static void
ide_setup_int(uint32_t channel, uint32_t irq)
{
        ide_write(channel, IDE_REG_CONTROL, 0);

        /* TODO: only for legacy mode now */
        ioapic_legacy_irq_activate(irq);

        idt_flag_t flag = IDT_PRESENT | IDT_32B_INT; 
        idt_create(idt_entries + IRQ_OFFSET + irq, (uintptr_t) ide_handler, flag);
}

static void
ide_reset(void)
{
        ide_write(IDE_PRIMARY, IDE_REG_CONTROL, (1 << 2));
        ide_write(IDE_PRIMARY, IDE_REG_CONTROL, 0);
}

void
ide_init(void)
{
        if (!is_ide_set) {
                kprintf("there isn't any IDE controller\n");
                return;
        }

        for (uint32_t channel = 0; channel < 2; ++channel) 
                for (uint32_t drive = 0; drive < 2; ++drive) 
                        ide_detect_drive(channel, drive);

        for (uint32_t i = 0; i < 4; ++i) {
                if (!ide_devices[i].present) continue;

                kprintf("Found %s Drive %dMB - %s\n",
                        (const char *[]){"ATA", "ATAPI"}[ide_devices[i].type],
                        ide_devices[i].size / 1024 / 2,
                        ide_devices[i].model);
        }

        ide_setup_int(IDE_PRIMARY, 14);
        ide_setup_int(IDE_SECONDARY, 15);
        ide_reset();

        kprintf("IDE init success\n");
}
