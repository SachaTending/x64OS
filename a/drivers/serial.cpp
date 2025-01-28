#include <libc.h>
#include <io.h>
#include <logging.hpp>

static Logger log("Serial");

class Serial {
    private:
    uint16_t io_addr;
    bool enabled;
    public:
        Serial(uint16_t io_addr) {
            this->io_addr = io_addr;
            outb(this->io_addr + 1, 0x00);    // Disable all interrupts
            outb(this->io_addr + 1, 0x00);    // Disable all interrupts
            outb(this->io_addr + 3, 0x80);    // Enable DLAB (set baud rate divisor)
            outb(this->io_addr + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
            outb(this->io_addr + 1, 0x00);    //                  (hi byte)
            outb(this->io_addr + 3, 0x03);    // 8 bits, no parity, one stop bit
            outb(this->io_addr + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
            outb(this->io_addr + 4, 0x0B);    // IRQs enabled, RTS/DSR set
            outb(this->io_addr + 4, 0x1E);    // Set in loopback mode, test the serial chip
            outb(this->io_addr + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

            // Check if serial is faulty (i.e: not same byte as sent)
            if(inb(this->io_addr  + 0) != 0xAE) {
                log.error("Failed to init serial port at io address 0x%04x, does it even exists?\n", io_addr);
                this->enabled = true;
                return;
            }

            // If serial is not faulty set it in normal operation mode
            // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
            outb(this->io_addr  + 4, 0x0F);
            outb(this->io_addr  + 4, 0x0F);
            log.info("Serial port at io address 0x%04x successfully inititalized!\n", io_addr);
            this->enabled = true;
        }
        int is_transmit_empty() {
            if (this->enabled) {
                return inb(this->io_addr + 5) & 0x20;
            }
            return 0;
        }
        void write(uint8_t byte) {
            if (this->enabled == true) {
                while(is_transmit_empty() == 0){};
                outb(this->io_addr, byte);
            }
        }
        int serial_received() {
            if (this->enabled) {
                return inb(this->io_addr + 5) & 1;
            }
            return 0;
        }

        uint8_t read_serial() {
            if (this->enabled) {
                while (serial_received() == 0);

                return inb(this->io_addr);
            }
        }
};

Serial com1(0x3f8);

uint8_t read_com1() {
    return com1.read_serial();
}