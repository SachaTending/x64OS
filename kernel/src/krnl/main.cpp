#include <krnl.hpp>
#include <vfs.hpp>
#include <libc.h>
#include <arch/vmm.h>
#include <sched/sched.hpp>
#include <prg_loading.hpp>
#include <stddef.h>
#include <printf/printf.h>
#include <arch/io.h>
#include <uacpi/uacpi.h>
#include <uacpi/tables.h>
#include <acpi.h>
#include <arch/arch.hpp>
#include <config.h>
#include <rng.hpp>

#ifdef CONFIG_SPECIAL_EDITION
#define CURRENT_YEAR        2025                            // Change this each year!

int century_register = 0x00;                                // Set by ACPI table parsing code if possible

unsigned char second;
unsigned char minute;
unsigned char hour;
unsigned char day;
unsigned char month;
unsigned int year;
#define out_byte outb
#define in_byte inb

enum {
      cmos_address = 0x70,
      cmos_data    = 0x71
};

int get_update_in_progress_flag() {
      out_byte(cmos_address, 0x0A);
      return (in_byte(cmos_data) & 0x80);
}

unsigned char get_RTC_register(int reg) {
      out_byte(cmos_address, reg);
      return in_byte(cmos_data);
}

void read_rtc() {
      unsigned char century;
      unsigned char last_second;
      unsigned char last_minute;
      unsigned char last_hour;
      unsigned char last_day;
      unsigned char last_month;
      unsigned char last_year;
      unsigned char last_century;
      unsigned char registerB;

      // Note: This uses the "read registers until you get the same values twice in a row" technique
      //       to avoid getting dodgy/inconsistent values due to RTC updates

      while (get_update_in_progress_flag());                // Make sure an update isn't in progress
      second = get_RTC_register(0x00);
      minute = get_RTC_register(0x02);
      hour = get_RTC_register(0x04);
      day = get_RTC_register(0x07);
      month = get_RTC_register(0x08);
      year = get_RTC_register(0x09);
      if(century_register != 0) {
            century = get_RTC_register(century_register);
      }

      do {
            last_second = second;
            last_minute = minute;
            last_hour = hour;
            last_day = day;
            last_month = month;
            last_year = year;
            last_century = century;

            while (get_update_in_progress_flag());           // Make sure an update isn't in progress
            second = get_RTC_register(0x00);
            minute = get_RTC_register(0x02);
            hour = get_RTC_register(0x04);
            day = get_RTC_register(0x07);
            month = get_RTC_register(0x08);
            year = get_RTC_register(0x09);
            if(century_register != 0) {
                  century = get_RTC_register(century_register);
            }
      } while( (last_second != second) || (last_minute != minute) || (last_hour != hour) ||
               (last_day != day) || (last_month != month) || (last_year != year) ||
               (last_century != century) );

      registerB = get_RTC_register(0x0B);

      // Convert BCD to binary values if necessary

      if (!(registerB & 0x04)) {
            second = (second & 0x0F) + ((second / 16) * 10);
            minute = (minute & 0x0F) + ((minute / 16) * 10);
            hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
            day = (day & 0x0F) + ((day / 16) * 10);
            month = (month & 0x0F) + ((month / 16) * 10);
            year = (year & 0x0F) + ((year / 16) * 10);
            if(century_register != 0) {
                  century = (century & 0x0F) + ((century / 16) * 10);
            }
      }

      // Convert 12 hour clock to 24 hour clock if necessary

      if (!(registerB & 0x02) && (hour & 0x80)) {
            hour = ((hour & 0x7F) + 12) % 24;
      }

      // Calculate the full (4-digit) year

      if(century_register != 0) {
            year += century * 100;
      } else {
            year += (CURRENT_YEAR / 100) * 100;
            if(year < CURRENT_YEAR) year += 100;
      }
}
#endif

static Logger *log = new Logger("Kernel");
extern bool p;
void unpack_initrd();
void load_lol(resource *res, pagemap *pgm, uint64_t *entry);
typedef void (*c)();
#define PRG "/kexec"
const char *argv[] = {PRG, "/kexec", NULL};
const char *envp[] = {"HOME=/", NULL};
#ifdef CONFIG_SPECIAL_EDITION
void countdown() {
    asm volatile ("cli");
    // No interrupts bcz we just gonna read cmos registers to get time and print it.
    // Fetch FADT to get century register
    acpi_fadt *fadt;
    FADT *fadt2;
    uacpi_status r = uacpi_table_fadt(&fadt);
    if (uacpi_unlikely_error(r)) {
        log->warn("Failed to find FACP table: %s\n", uacpi_status_to_string(r));
    } else {
        fadt2 = (FADT *)fadt;
        century_register = fadt2->Century;
        log->debug("century reg: %d\n", century_register);
    }
    bool printed = false;
    int last_sec = 0;
    while (1) {
        read_rtc();
        if (last_sec != second) {
            last_sec = second;
            int seconds_remaining = 60 - second;
            int minutes_remaining = 60 - minute;
            int hour_remaining = 23 - hour;
            printf("%02d:%02d:%02d %02d.%02d.%04d, %02d:%02d:%02d until new year\n", hour, minute, second, day, month, year, hour_remaining, minutes_remaining, seconds_remaining);
        } if (year == 2026 and printed == false) {
            printf("Happy New Year!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
            printed = true;
        }
    }
}
#endif
int g_errno; // idk
extern "C" int *__errno_location(void) {
    return &g_errno;
}
void fbdev_init();
void console_init(void);
void Kernel::Main() {
    p = true;
    log->info("Kernel::Main(); started.\n");
    log->info("%s\n", full_ver);
    log->info("idk what to put here, but this is a Kernel::Main\n");
   // log->info("Starting countdown...\n");
    //countdown();
    VFS::Init();
    VFS::Mount(vfs_root, NULL, "/", "tmpfs");
    VFS::Create(vfs_root, "/dev", 0755 | S_IFDIR);
    VFS::Mount(vfs_root, NULL, "/dev", "devtmpfs");
    console_init();
    fbdev_init();
    unpack_initrd();
    vfs_node_t *node;
    #if CONFIG_TEST_VFS=='y'
    log->info("Trying to read file from VFS...\n");
    node = VFS::GetNode(vfs_root, "/hi.txt", true);
    if (node == NULL) {
        log->error("Failed to get file, is it unpacked?\n");
    } else {
        log->info("Successfully opened file /hi.txt!\n");
        log->info("File contents: ");
        char buf[16384];
        memset(buf, 0, 200);
        node->resource->read(node->resource, NULL, buf, 0, 180);
        printf(buf);
        printf("\n");
        log->info("File has been read successfully\n");
    }
    #endif
    //log->info("Легро, где арты?\n");
    //#define PRG "/linux_compat_layer_test"
    log->info("gonna launch busybox fbset\n");
    node = VFS::GetNode(vfs_root, PRG, true);
    log->info("node 0x%lx\n", node);
    if (node && 1) {
        //log->info("node 0x%lx opened\n", node);
        pagemap *pgm = vmm_new_pagemap();
        auxval aux, ld_auxv;
        const char *ld;
        bool ret = elf_load(pgm, node->resource, 0x0, &aux, &ld);
        uint64_t prg_entry = aux.at_entry;
        if (ld != 0) {
            vfs_node_t *ld_open = VFS::GetNode(vfs_root, ld, true);
            if (ld_open == NULL) {
                log->error("Failed to load linker %s for %s: File not found\n", ld, PRG);
                PANIC("Failed to start %s as init program.\n", PRG);
            }
            ret = elf_load(pgm, ld_open->resource, 0x40000000, &ld_auxv, NULL);
            if (ret == false) {
                log->error("Failed to load %s linker for %s\n", ld, PRG);
                PANIC("Failed to start %s as init program.\n", PRG);
            }
            prg_entry = ld_auxv.at_entry;
        }
        //ret = elf_load(pgm, ld_open->resource, 0x40000000, &ld_auxv, NULL);
        if (ret == false) {
            log->error("Failed to load %s as elf program.\n", PRG);
            PANIC("Failed to start %s as init program.\n", PRG);
        }
        log->info("%s info:\n", PRG);
        log->info("entry: 0x%lx\n", prg_entry);
        if (ld) {
            log->info("interpreter: %s\n", ld);
        } else {
            log->info("No interpreter.\n");
        }
        Scheduler::Stop();
        log->info("pgm: 0x%016lx\n", pgm);
        Scheduler::CreateThread(PRG, (void (*)())prg_entry, true, pgm, argv, envp, &aux);
        Scheduler::Start();
    } else {
        node = VFS::GetNode(vfs_root, "/dev/fb0", true);
        uint32_t a = 0;
        uint32_t shift = 0;
        for (int i=0;i<1000000;i++) {
            node->resource->write(node->resource, 0, &a, i*4, 4);
            a ^= rand() & (0xf << shift);
            shift += 4;
            if (shift == 24) shift=0;
        }
    }
    while (1);
}

__attribute__((noreturn)) void panic(const char *file, size_t lnum, const char *msg, ...) {
    STOP_INTERRUPTS;
    printf("KERNEL PANIC. At %s:%lu\nReason: ", file, lnum);
    va_list lst;
    va_start(lst, msg);
    vprintf(msg, lst);
    va_end(lst);
    putchar_('\n');
    //stacktrace(0);
    printf("\nSystem halted.\n");
    HCF;
    __builtin_unreachable();
}