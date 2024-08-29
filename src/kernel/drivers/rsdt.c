#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <kernel/debug.h>
#include <kernel/rsdt.h>
#include <kernel/page.h>
#include <kernel/memory.h>

static rsdt_t *rsdt;

static rsdp20_descriptor_t*
rsdp_check(uintptr_t start_addr, uintptr_t end_addr)
{
        uint8_t *start = (uint8_t*) start_addr;
        uint8_t *end = (uint8_t*) end_addr;
        
        /* the signature is 16 byte aligned */
        for (; start < end; start += 16)
                if (!memcmp(start, RSDP_SIGNATURE, RSDP_SIGNATURE_SIZE))
                        return (rsdp20_descriptor_t*) start;
                
        return NULL;
}

static rsdp20_descriptor_t*
rsdp_find(void)
{
        /* it's only needed to check the first KiB of the EBDA */
        rsdp20_descriptor_t *descriptor;
        descriptor = rsdp_check(EBDA_START, EBDA_START + KIB(1));
        
        if (descriptor)
                return descriptor;

        /* checking in the main bios area (0xE0000 - 0xFFFFF) */
        descriptor = rsdp_check(MAIN_BIOS_START, MAIN_BIOS_END);

        if (descriptor)
                return descriptor;

        return NULL;
}

/* return 1 if checksum passed, otherwise return 0 */
static int
rsdp_checksum(rsdp20_descriptor_t *rsdp_descriptor, uint8_t version)
{
        uint8_t sum = 0;
        uint8_t *start = (uint8_t*) rsdp_descriptor;
        uint8_t *end = start + sizeof(rsdp_descriptor_t);

        for (; start < end; ++start)
                sum += *start;
        
        /* if ACPI is the first version (version = 0) return result immediately 
           or if the result is negative don't waste time and return result */
        if (!version || (sum & 1)) return ~sum & 1;

        sum = 0;
        /* uint8_t *start is already pointing at the start of second part */
        end = start + sizeof(rsdp20_descriptor_t) - sizeof(rsdp_descriptor_t);

        for (; start < end; ++start)
                sum += *start;

        return ~sum & 1;
}

static int
rsdt_checksum(void)
{
        uint8_t sum = 0;
        uint8_t *start = (uint8_t*) rsdt;
        uint8_t *end = start + rsdt->header.length;

        for (; start < end; ++start)
                sum += *start;

        /* sum should equal to zero, it's possible by overflow */
        return sum == 0;
}

std_header_t*
rsdt_get_entry(const char *signature)
{
        std_header_t **entry = &rsdt->first_entry;

        size_t entries = (rsdt->header.length - sizeof(rsdt->header)) / sizeof(std_header_t*);
        std_header_t **last = entry + entries;

        for (; entry < last; ++entry)
               if (!memcmp((*entry)->signature, signature, RSDT_SIGNATURE_SIZE)) {
                        DPRINTF("[RSDT] entry %s found at addr %x\n", signature, entry);
                        return *entry;
               }

        DPRINTF("[RSDT] couldn't find entry %s\n", signature);
        return NULL;
}

void 
rsdt_init(void)
{
        kprintf("[RSDT] setup STARTING\n");
        
        rsdp20_descriptor_t *rsdp_descriptor = rsdp_find();

        if (!rsdp_descriptor) {
                kprintf("[RSDT] can't find RSDT address\n");
                return;
        }

        if (!rsdp_checksum(rsdp_descriptor, rsdp_descriptor->original.revision)) {
                kprintf("[RSDT] RSDP checksum FAILED\n");
                return;
        }
        
        uintptr_t rsdt_addr = (rsdp_descriptor->original.revision) ?
                (uintptr_t) rsdp_descriptor->xsdt_addr :
                (uintptr_t) rsdp_descriptor->original.rsdt_addr;

        rsdt = (rsdt_t*) rsdt_addr;
        page_identity_map(page_directory, rsdt_addr, sizeof(rsdt_t));
        
        if (!rsdt_checksum()) {
                kprintf("RSDT checksum FAILED\n");
                return;
        }

        kprintf("[RSDT] setup COMPLETE\n");
}
