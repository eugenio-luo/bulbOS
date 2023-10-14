#ifndef _KERNEL_RSDT_H
#define _KERNEL_RSDT_H

#include <stdint.h>

#define RSDP_SIGNATURE        "RSD PTR "
#define RSDP_SIGNATURE_SIZE   8
#define RSDT_SIGNATURE_SIZE   4

typedef struct {
        char signature[RSDP_SIGNATURE_SIZE];
        uint8_t checksum;
        uint8_t oemid[6];
        uint8_t revision;
        uint32_t rsdt_addr;
} __attribute__ ((packed)) rsdp_descriptor_t;

typedef struct {
        rsdp_descriptor_t original;

        uint32_t length;
        uint64_t xsdt_addr;
        uint8_t ext_checksum;
        uint8_t reserved[3];
} __attribute__ ((packed)) rsdp20_descriptor_t;

/* it should be aligned on 32 bit */
typedef struct {
        uint8_t signature[RSDT_SIGNATURE_SIZE];
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        uint8_t oemid[6];
        uint8_t oem_tableid[8];
        uint32_t oem_revision;
        uint32_t creatorid;
        uint32_t creator_revision;
} std_header_t;

/* it should be aligned on 32 bit */
typedef struct {
        std_header_t header;
        std_header_t *first_entry;
} rsdt_t;

void rsdt_init(void);
std_header_t* rsdt_get_entry(const char *signature);

#endif
