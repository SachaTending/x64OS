#include <limine.h>
#include <logging.hpp>

static Logger log("SMP");

limine_smp_request smp_req = {
    .id = LIMINE_SMP_REQUEST,
    .revision = 0,
    .response = 0,
    .flags = 0
};

struct smp_task_t {
    void (*task_func)();
    bool avaible = false;
    bool cpu_ready = false;
};

smp_task_t tasks[64];

uint64_t bsp_lapic_id;

void smp_entry(limine_smp_info *smp) {
    int cid = smp->lapic_id;
    tasks[cid].cpu_ready = true;
    while (1)
    {
        if (tasks[cid].avaible) {
            tasks[cid].task_func();
            tasks[cid].avaible = false;
        }
    }
    
}

void smp_init() {
    log.info("CPU count: %u\n", smp_req.response->cpu_count);
    bsp_lapic_id = smp_req.response->bsp_lapic_id;
    int cpu_id = 0;
    tasks[smp_req.response->bsp_lapic_id].cpu_ready = true;
    while (cpu_id <= smp_req.response->cpu_count-1 & cpu_id < sizeof(tasks)/sizeof(smp_task_t)) {
        log.debug("Starting cpu %d...\n", cpu_id+1);
        for (int i=0;i<1000;i++) smp_req.response->cpus[cpu_id]->goto_address = smp_entry;
        cpu_id += 1;
    }
}