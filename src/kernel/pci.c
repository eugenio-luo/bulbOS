#include <stdint.h>
#include <stdio.h>

#include <kernel/pci.h>
#include <kernel/ide.h>

static void pci_check_bus(uint32_t bus);

void
pci_write_reg(uint32_t bus, uint32_t device, uint32_t func,
              uint32_t offset, uint16_t data)
{
        uint32_t address = (bus << 16) | (device << 11) |
                (func << 8) | (offset & 0xFC) | (1 << 31);

        outl(address, CONFIG_ADDR);
        outw(data, CONFIG_DATA);
}

uint32_t
pci_read_reg(uint32_t bus, uint32_t device, uint32_t func, uint32_t offset)
{
        uint32_t address = (bus << 16) | (device << 11) |
                (func << 8) | (offset & 0xFC) | (1 << 31);

        outl(address, CONFIG_ADDR);
        return inl(CONFIG_DATA);
}

static void
pci_check_function(uint32_t bus, uint32_t device, uint32_t func)
{
        uint32_t pci_classes = pci_read_reg(bus, device, func, 2 << 2);
        uint32_t class = pci_classes >> 24;
        uint32_t subclass = (pci_classes >> 16) & 0xFF;
        uint32_t prog_if = (pci_classes >> 8) & 0xFF;
        
        kprintf("device: ", class, subclass, prog_if);

        switch (class) {
        case 1: 
                switch (subclass) {
                case 1: {
                        ide_set(bus, device, func);
                        uint16_t pci_command_reg = pci_read_reg(bus, device, func, 1 << 2) & 0xFFFF;
                        pci_write_reg(bus, device, func, 0x6, pci_command_reg | (1 << 2));
                        kprintf("IDE controller\n");
                        break;
                }
                }
                break;

        case 0xFF:
                kprintf("Unassigned class\n");
                break;
                
        default:
                kprintf("\n");
                break;
        }
        
        if ((class != 0x6) || (subclass != 0x4)) return;

        uint32_t reg = pci_read_reg(bus, device, func, 6 << 2);
        uint32_t secondary_bus = (reg >> 8) & 0xFF;
        pci_check_bus(secondary_bus);
}

static void
pci_check_device(uint32_t bus, uint32_t device)
{
        uint32_t vendor = pci_read_reg(bus, device, 0, 0) & 0xFFFF;
        if (vendor == 0xFFFF) return;
                
        pci_check_function(bus, device, 0);
        uint32_t pci_header = (pci_read_reg(bus, device, 0, 3 << 2) >> 16) & 0xFF;
        if (~pci_header & 0x80) return;

        for (int func = 1; func < 8; ++func) {
                vendor = pci_read_reg(bus, device, func, 0);
                if (vendor == 0xFFFF) break;

                pci_check_function(bus, device, func);
        }
}

static void
pci_check_bus(uint32_t bus)
{
        for (int device = 0; device < 32; ++device) {
                pci_check_device(bus, device);
        }
}

static void
pci_check_all_bus(void)
{
        uint32_t pci_header = (pci_read_reg(0, 0, 0, 3 << 2) >> 16) & 0xFF;
        pci_check_bus(0);
        if (~pci_header & 0x80) return;

        for (int func = 1; func < 8; ++func) {
                uint32_t vendor = pci_read_reg(0, 0, func, 0) & 0xFFFF;
                if (vendor != 0xFFFF) break;

                pci_check_bus(func);
        }
}

void
pci_init(void)
{
        pci_check_all_bus();
        
        kprintf("PCI setup SUCCESS\n");
}
