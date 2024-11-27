#pragma once
#include <stdint.h>
class Serial {
    private:
        uint16_t io_addr;
        bool enabled;
    public:
        Serial(uint16_t io_addr);
        int is_transmit_empty();
        void write(uint8_t byte);
        int serial_received();
        uint8_t read_serial();
};