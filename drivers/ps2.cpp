#include <io.h>
#include <logging.hpp>
#include <idt.hpp>
#include <acpi.hpp>

static Logger log("PS/2(Not a game console)");

#define PS2_DATA 0x60
#define PS2_STATUS_COMMAND 0x64

#define CMD_READ_CONFIG_BYTE    0x20
#define CMD_WRITE_CONFIG_BYTE   0x60  
#define CMD_SELF_TEST
#define CMD_DISABLE_SECOND_PORT 0xA7
#define CMD_ENABLE_SECOND_PORT  0xA8
#define CMD_TEST_SECOND_PORT    0xA9
#define CMD_TEST_CONTROLLER     0xAA
#define CMD_TEST_FIRST_PORT     0xAB
#define CMD_DISABLE_FIRST_PORT  0xAD
#define CMD_ENABLE_FIRST_PORT   0xAE

#define BIT(n) (1 << n)
#define CLEAR_BIT(number, bit) (number & ~(1 << bit))

#define BIT_OUTPUT_BUFFER BIT(0)
#define BIT_INPUT_BUFFER  BIT(1)

#define CHECK_STATUS_BIT(n) (inb(PS2_STATUS_COMMAND) & n)

#define TIMEOUT 10000

uint8_t ps2_send_cmd(uint8_t cmd) {
    while (CHECK_STATUS_BIT(BIT_INPUT_BUFFER));
    outb(PS2_STATUS_COMMAND, cmd);
    log.debug("sent cmd to ps2 controller: 0x%x\n", cmd);
    uint32_t timeout=TIMEOUT;
    while (CHECK_STATUS_BIT(BIT_OUTPUT_BUFFER) == 0) {
        timeout--;
        if (timeout <= 0) {
            return 0;
        }
    }
    return inb(0x60);
}

uint8_t ps2_send_cmd2(uint8_t cmd, uint8_t data) {
    while (CHECK_STATUS_BIT(BIT_INPUT_BUFFER));
    outb(PS2_STATUS_COMMAND, cmd);
    while (CHECK_STATUS_BIT(BIT_INPUT_BUFFER));
    outb(PS2_DATA, data);
    uint32_t timeout=TIMEOUT;
    while (CHECK_STATUS_BIT(BIT_OUTPUT_BUFFER) == 0) {
        timeout--;
        if (timeout <= 0) {
            return 0;
        }
    }
    return inb(0x60);
}

void ps2_flush_data() {
    while (CHECK_STATUS_BIT(BIT_OUTPUT_BUFFER)) {
        inb(0x60);
    }
}

void ps2_send_dev(uint8_t byte) {
    while (CHECK_STATUS_BIT(BIT_INPUT_BUFFER));
    outb(PS2_DATA, byte);
}
static uint8_t _ps2_data_1 = 0;
static bool _ps2_data_1_received = false;
uint8_t ps2_recv_dev() {
    int timeout = 1;
    while (_ps2_data_1_received == false) {
        timeout--;
        asm volatile ("hlt");
        if (timeout <= 0) {
            //log.debug("warning: timeout on read\n");
            return 0;
        }
    }
    _ps2_data_1_received = false;
    return _ps2_data_1;
}

void ps2_int_1(idt_regs *regs) {
    (void)regs;
    _ps2_data_1 = inb(0x60);
    _ps2_data_1_received = true;
    log.debug("ps2 data: 0x%x\n", _ps2_data_1);
}

static uint8_t port_count = 1;
void init_ps2() {
    // Check if PS/2 controller exists
    FADT *fadt = (FADT *)find_table("FACP", 0);
    if (fadt != 0) {
        if ((fadt->BootArchitectureFlags & BIT(1)) == 1) { // No PS/2 controller
            log.error("No PS/2 contoller found, are you running this on apple machine?\n");
            return;
        }
    }
    ps2_send_cmd(CMD_DISABLE_FIRST_PORT); ps2_send_cmd(CMD_DISABLE_SECOND_PORT);
    ps2_flush_data();
    uint8_t config = ps2_send_cmd(CMD_READ_CONFIG_BYTE);
    config = CLEAR_BIT(CLEAR_BIT(config, 0), 6);
    ps2_send_cmd2(CMD_WRITE_CONFIG_BYTE, config);
    uint8_t resp = ps2_send_cmd(0xAA); // Perform self test
    if (resp != 0x55) {
        log.error("There is something wrong with PS/2 controller, cannot continue initialization of controller.\n");
        return;
    }
    ps2_send_cmd(CMD_ENABLE_SECOND_PORT);
    config = ps2_send_cmd(CMD_READ_CONFIG_BYTE);
    if ((config & BIT(5)) == 0) {
        log.info("PS/2 Controller has 2 ports.\n");
        port_count = 2;
        config = CLEAR_BIT(CLEAR_BIT(config, 1), 5);
        ps2_send_cmd(CMD_DISABLE_SECOND_PORT);ps2_send_cmd2(CMD_WRITE_CONFIG_BYTE, config);
    } else {
        log.info("PS/2 Controller has 1 port.\n");
    }
    uint8_t result = ps2_send_cmd(CMD_TEST_FIRST_PORT);
    switch (result)
    {
        case 0:
            log.info("First port tested, result: OK\n");
            break;
        case 0x1:
            log.error("First port test fail: Clock line stuck low\n");
            return;
            break;
        case 0x2:
            log.error("First port test fail: Clock line stuck high\n");
            return;
            break;
        case 0x3:
            log.error("First port test fail: Data line stuck low\n");
            return;
            break;;
        case 0x4:
            log.error("First port test fail: Data line stuck high\n");
            return;
            break;
        default:
            log.error("First port test fail? Got unknown result: 0x%x(maybe chipset specific?)\n", result);
            return;
            break;
    }
    ps2_send_cmd(CMD_ENABLE_FIRST_PORT);
    config = ps2_send_cmd(CMD_READ_CONFIG_BYTE);
    config |= BIT(0);
    idt_set_int(1, ps2_int_1);
    ps2_send_cmd2(CMD_WRITE_CONFIG_BYTE, config);
    ps2_send_dev(0xFF);
    uint8_t data = ps2_recv_dev();
    if ((data == 0xFA) || (data == 0xAA)) {
        log.info("PS/2 Controller and device on first port successfully initialized!\n");
        ps2_flush_data();
    } else {
        log.error("PS/2 Contoller succesfully initialized but there is something wrong with device.\n");
        log.debug("response=0x%x\n", data);
    }
}
