#ifndef _KERNEL_PCI_H
#define _KERNEL_PCI_H

#define CONFIG_ADDR 0xCF8
#define CONFIG_DATA 0xCFC

void pci_init(void);
void pci_write_reg(uint32_t bus, uint32_t device, uint32_t func,
                   uint32_t offset, uint16_t data);
uint32_t pci_read_reg(uint32_t bus, uint32_t device, uint32_t func, uint32_t offset);

#endif
