#include <stddef.h>
#include <libc.h>
extern "C" {
    size_t strlen(const char *str) {
        size_t o = 0;
        while (*str++)
        {
            o++;
        }
        return o;
    }
    void strcpy(char *dst, const char *src) {
        size_t len = strlen(src);
        for (size_t i=0;i<len;i++) {
            dst[i] = src[i];
        }
    }
    void itoa(char *buf, unsigned long int n, int base)
    {
        unsigned long int tmp;
        int i, j;

        tmp = n;
        i = 0;

        do {
            tmp = n % base;
            buf[i++] = (tmp < 10) ? (tmp + '0') : (tmp + 'a' - 10);
        } while (n /= base);
        buf[i--] = 0;

        for (j = 0; j < i; j++, i--) {
            tmp = buf[j];
            buf[j] = buf[i];
            buf[i] = tmp;
        }
    }
    int atoi(char * string) {
        int result = 0;
        unsigned int digit;
        int sign;

        while (isspace(*string)) {
            string += 1;
        }

        /*
        * Check for a sign.
        */

        if (*string == '-') {
            sign = 1;
            string += 1;
        } else {
            sign = 0;
            if (*string == '+') {
                string += 1;
            }
        }

        for ( ; ; string += 1) {
            digit = *string - '0';
            if (digit > 9) {
                break;
            }
            result = (10*result) + digit;
        }

        if (sign) {
            return -result;
        }
        return result;
    }

    int isspace(char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
    }

}