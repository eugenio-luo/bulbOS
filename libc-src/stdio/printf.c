#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/*
 *  uint8_t flags:
 *  bit 0 = alternate form, BROKEN???
 *  bit 1 = zero-padded
 *  bit 2 = left adjusted, NOT IMPLEMENTED YET
 *  bit 3 = signed, NOT IMPLEMENTED YET
 *  bit 4 - 7 = field width (max 16), NOT IMPLEMENTED YET
 */

#define ALTERNATE_FORM 0
#define ZERO_PADDED    1
#define LEFT_ADJUSTED  2

static bool
print(const char* data, size_t length)
{
	const unsigned char* bytes = (const unsigned char*) data;
	for (size_t i = 0; i < length; i++)
		if (putchar(bytes[i]) == EOF)
			return false;
	return true;
}

static int
no_format_case(const char* restrict *format, size_t maxrem, int *written)
{
        if ((*format)[0] == '%') {
                format++;
        }

        size_t amount = 1;
        while ((*format)[amount] && (*format)[amount] != '%') {
                amount++;
        }
        
        if (maxrem < amount) {
                return 1;
        }
        
        if (!print(*format, amount)) return 1;
        *format += amount;
        *written += amount;
        return 0;
}

static int
c_format_case(const char* restrict *format, char c, size_t maxrem, int *written)
{
        (*format)++;
        if (!maxrem) {
                return 1;
        }
        if (!print(&c, sizeof(c))) return 1;
        ++(*written);
        return 0;
}

static int
s_format_case(const char* restrict *format, const char *str, size_t maxrem, int *written)
{
        (*format)++;
        size_t len = strlen(str);
        if (maxrem < len) {
                return 1;
        }
        if (!print(str, len)) return 1;
        *written += len;       
        return 0;
}

static int
num_format_case(const char* restrict *format, uint32_t value,
                size_t maxrem, int *written, int base, uint8_t flags)
{
        (*format)++;
        int init_index = 0;
        char buffer[30];
        /*
        char padding;

        if ((flags >> ZERO_PADDED & 0x1)) {
                padding = '0';
        }
        */
        
        if (base == 16 && (flags >> ALTERNATE_FORM & 0x1)) {
                buffer[init_index++] = '0';
                buffer[init_index++] = 'x';
        }
        
        size_t len = uitoa(value, &buffer[init_index], base);

        /*
        if ((flags >> ZERO_PADDED & 0x1) && len <= 8) {
                size_t diff = 8 - len;
                printf("im here, diff: %d, %d\n", diff, len);
                memmove(&buffer[init_index + diff], &buffer[init_index], len);

                for (size_t i = 0; i < diff; ++i) {
                        buffer[init_index + i] = padding;
                }
        }
        */

        if (maxrem < len) {
                return 1;
        }
        
        if (!print(buffer, len)) return 1;
        *written += len;
        return 0;
}

int32_t
vprintf(const char* restrict format, va_list parameters)
{
        int written = 0;

	while (*format != '\0') {
		size_t maxrem = INT_MAX - written;
                uint8_t flags = 0;

                if (format[0] != '%' || format[1] == '%') {
			int ret = no_format_case(&format, maxrem, &written);
                        if (ret == 1) return -1;
                        continue;
                }

		const char* format_begun_at = format++;

                int break_from_while = 0;
                while (!break_from_while) {
                        switch (*format) {
                        case '0':
                                flags |= 1 << ZERO_PADDED;
                                ++format;
                                break;

                        case '#':
                                flags |= 1 << ALTERNATE_FORM;
                                ++format;
                                break;
                        
                        default:
                                break_from_while = 1;
                                break;
                        }
                }

                int ret = 0;

                switch (*format) {
                case 'c': {
                        char c = (char) va_arg(parameters, int);
                        ret = c_format_case(&format, c, maxrem, &written); 
                        break;
                }
                case 's': {
                        const char *str = va_arg(parameters, const char*);
                        ret = s_format_case(&format, str, maxrem, &written); 
                        break;
                }
                case 'd': {
                        uint32_t value = va_arg(parameters, uint32_t);
                        ret = num_format_case(&format, value, maxrem, &written, 10, flags);
                        break;
                }
                case 'x': {
                        uint32_t value = va_arg(parameters, uint32_t);
                        ret = num_format_case(&format, value, maxrem, &written, 16, flags);
                        break;
                }

                default: {
                        format = format_begun_at;
			            size_t len = strlen(format);
			            if (maxrem < len) {
				                return -1;
			            }
			            if (!print(format, len))
				                return -1;
			            written += len;
			            format += len;
                        break;
                }
                }

                if (ret == 1) return -1;
        }

        return written;
}

int32_t
printf(const char* restrict format, ...)
{
	va_list parameters;
	va_start(parameters, format);

        setoutput(TERMINAL_OUT);
	int ret = vprintf(format, parameters);

	va_end(parameters);
	return ret;
}

int32_t
kprintf(const char* restrict format, ...)
{
	va_list parameters;
	va_start(parameters, format);

        setoutput(SERIALPORT_OUT);
        int ret = vprintf(format, parameters);
        va_end(parameters);
        
        setoutput(TERMINAL_OUT);
        return ret;
}
