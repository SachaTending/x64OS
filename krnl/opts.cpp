#include <limine.h>
#include <new>
#include <stddef.h>
#include <frg/std_compat.hpp>
#include <frg/vector.hpp>
#include <logging.hpp>
#include <libc.h>

static Logger log("Options parser");

extern limine_kernel_file_request limine_krnl_file;

typedef struct {
    const char *opt_name;
    const char *opt_val;
    bool is_no_val;
} opt_t;

typedef frg::vector<opt_t, frg::stl_allocator> opt_array;

opt_array opts_array = opt_array();

#define find_chr(chr, def, repl, aa) int def = int(strchr(aa, chr));if(def==0){def=repl+int(aa);};def-=int(aa)

void parse_opts() {
    log.info("Parsing options...\n");
    log.info("Kernel command line: %s\n", limine_krnl_file.response->kernel_file->cmdline);
    size_t cmdline_len = strlen((const char *)limine_krnl_file.response->kernel_file->cmdline);
    const char *cmdline_pntr = limine_krnl_file.response->kernel_file->cmdline;
    while (cmdline_pntr < (limine_krnl_file.response->kernel_file->cmdline + cmdline_len) && cmdline_pntr != (limine_krnl_file.response->kernel_file->cmdline + cmdline_len)) {
        //int space_pos = int(strchr(cmdline_pntr, ' '));
        //if (space_pos == 0) space_pos = strlen(cmdline_pntr) + int(cmdline_pntr);
        //space_pos -= int(cmdline_pntr);
        find_chr(' ', space_pos, strlen(cmdline_pntr), cmdline_pntr);
        const char *opt = new char[space_pos+1];
        if (opt == 0) {
            PANIC("Cannot allocate memory for next option");
        }
        memset((void *)opt, 0, space_pos);
        memcpy((void *)opt, cmdline_pntr, space_pos);
        find_chr('=', eq_pos,0, opt);
        bool is_no_val = eq_pos ? 0:1;
        log.info("%s %d\n", opt, is_no_val);
        opt_t opt_struct = {
            .opt_name = opt,
            .opt_val = is_no_val?opt+eq_pos:0,
            .is_no_val = is_no_val
        };
        if (is_no_val == false) {
            ((char *)opt)[eq_pos] = 0;
        }
        opts_array.push_back(opt_struct);
        cmdline_pntr+=space_pos+1;
    }
}

const char *get_val(const char *opt) {
    size_t arr_size = opts_array.size();
    size_t i = 0;
    while (i < arr_size) {
        opt_t opt2 = opts_array[i];
        if (opt2.is_no_val) continue;
        if (!strcmp(opt2.opt_name, opt)) return opt2.opt_val;
        i++;
    }
    return NULL;
}

bool check_val(const char *opt) {
    size_t arr_size = opts_array.size();
    size_t i = 0;
    while (i < arr_size) {
        opt_t opt2 = opts_array[i];
        if (!strcmp(opt2.opt_name, opt)) return true;
    }
    return false;
}