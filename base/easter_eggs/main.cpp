#include <libc.h>

bool check_val(const char *opt);

typedef void (*handler_t)();

struct easter_egg_t {
    handler_t handler;
    const char *opt;
};
void easter_egg_test();
void easter_egg_averi();
static easter_egg_t arr[] = {
    {easter_egg_averi, "averi"},
    {(handler_t)-1, (const char *)-1}
};

void handle_eastereggs() {
    uint64_t ind = 0;
    while (arr[ind].opt != (const char *)-1) {
        if (check_val(arr[ind].opt)) {
            arr[ind].handler();
        }
        ind++;
    }
}