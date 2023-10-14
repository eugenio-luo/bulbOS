#include <string.h>
#include <stdint.h>

size_t
itoa(int value, char *str, int base)
{
        if (value == 0) {
                str[0] = '0', str[1] = '\0';
                return 1;
        }

        int len = 0;
        if (value < 0) {
                str[len++] = '-';
                value = value - (value * 2);
        }

        if (base == 16) {
                str[len++] = '0', str[len++] = 'x';
        }

        int tmp = value;
        while (tmp != 0) {
                ++len;
                tmp /= base;
        }
        
        int index = len - 1;
        while (value != 0) {
                int digit = value % base;
                if (digit > 9) {
                        str[index--] = digit - 10 + 'a';
                } else {
                        str[index--] = digit + '0';
                }

                value /= base;
        }

        str[len] = 0;
        return len;
}

size_t
uitoa(uint32_t value, char *str, uint32_t base)
{
        if (value == 0) {
                str[0] = '0', str[1] = '\0';
                return 1;
        }

        int len = 0;

        int tmp = value;
        while (tmp != 0) {
                ++len;
                tmp /= base;
        }

        int index = len - 1;
        while (value != 0) {
                int digit = value % base;
                if (digit > 9) {
                        str[index--] = digit - 10 + 'a';
                } else {
                        str[index--] = digit + '0';
                }

                value /= base;
        }

        str[len] = 0;
        return len;
}
