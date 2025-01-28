#pragma once
#include <stddef.h>

class String {
    public:
        String(const char *str);
        String();
        const char *to_char();
        ~String();
    private:
        const char *str;
        size_t size;
};