#pragma once
#include <stdint.h>

class Randomizer {
    public:
        virtual void set_seed(uint64_t seed);
        virtual uint64_t rand();
};

uint64_t rand();