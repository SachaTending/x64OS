volatile void create_task(int (*task)(), 
    const char *name, 
    bool usermode=false, 
    pagemap *pgm=krnl_page, 
    uint64_t tls=0, 
    int argc=0, 
    const char **argv=0, 
    const char **envp=0,
    auxval* aux=0) {
    _sched_stop_internal = true;
    spinlock_acquire(&sched_lock);
    task_t *new_task = new task_t;
    new_task->name = strdup(name);
    new_task->next = root_task;
    new_task->pid = next_pid;
    next_pid++;
    new_task->state = TASK_CREATE;
    new_task->regs.rip = (uint64_t)task_entry;
    new_task->regs.rax = (uint64_t)task;
    new_task->tls = tls;
    new_task->mmap_anon_base = 0x80000000000;
    //new_task->regs.rflags = 0x202;
    new_task->regs.rflags |= (1 << 9);
    volatile uintptr_t *stack;
    new_task->usermode = usermode;
    //#define stack (new_task->regs.rsp)
    if (usermode == true) {
        new_task->regs.rip = (uint64_t)task;
        new_task->regs.rax = 0;
        //new_task->regs.rcx = (uint64_t)task;
        new_task->regs.ds = new_task->regs.es = new_task->regs.ss = (7*8) | 3;
        new_task->regs.cs = (8*8) | 3;
        uint64_t rsp = (uint64_t)pmm_alloc(STACK_SIZE/4096);
        new_task->stack_addr = (void *)rsp;
        vmm_map_range(pgm, rsp, STACK_SIZE, PTE_PRESENT | PTE_USER | PTE_WRITABLE);
        //vmm_map_range(pgm, (uint64_t)task_entry-VMM_HIGHER_HALF, 8192, PTE_PRESENT | PTE_USER);
        new_task->regs.rsp = (rsp+STACK_SIZE);
        if (argc != NULL && argv != NULL && envp != NULL && aux != NULL) {
            vmm_switch_to(pgm);
            printf("pushing argc, argv, envp, aux...\n");
            printf("rsp: 0x%lx\n", new_task->regs.rsp);
            stack = (uintptr_t *)new_task->regs.rsp;
            void *stack_top = new_task->stack_addr+STACK_SIZE;
            int envp_len;
            for (envp_len = 0; envp[envp_len] != NULL; envp_len += 1) {
                size_t len = strlen(envp[envp_len]);
                stack = (uintptr_t *)((uint64_t)stack - len - 1);
                memcpy((void *)stack, envp[envp_len], len);
                printf("pushed env: %s\n", envp[envp_len]);
            }

            int argv_len;
            for (argv_len = 0; argv[argv_len] != NULL; argv_len++) {
                size_t len = strlen(argv[argv_len]);
                stack = (uintptr_t *)((uint64_t)stack - len - 1);
                memcpy((void *)stack, argv[argv_len], len);
                printf("pushed arg: %s\n", argv[argv_len]);
            }

            stack = (uintptr_t *)ALIGN_DOWN((uintptr_t)stack, 16);
            if (((argv_len + envp_len + 1) & 1) != 0) {
                stack--;
            }

            // Auxilary vector
            *(--stack) = 0, *(--stack) = 0;
            stack -= 2; stack[0] = AT_SECURE, stack[1] = 0;
            stack -= 2; stack[0] = AT_ENTRY, stack[1] = aux->at_entry;
            printf("at_entry=0x%lx\n", aux->at_entry);
            stack -= 2; stack[0] = AT_PHDR,  stack[1] = aux->at_phdr;
            stack -= 2; stack[0] = AT_PHENT, stack[1] = aux->at_phent;
            stack -= 2; stack[0] = AT_PHNUM, stack[1] = aux->at_phnum;

            uintptr_t old_rsp = new_task->regs.rsp;

             *(--stack) = 0;
            stack -= envp_len;
            for (int i = 0; i < envp_len; i++) {
                old_rsp -= strlen(envp[i]) + 1;
                stack[i] = old_rsp;
            }

            // Arguments
            *(--stack) = 0;
            stack -= argv_len;
            for (int i = 0; i < argv_len; i++) {
                old_rsp -= strlen(argv[i]) + 1;
                stack[i] = old_rsp;
            }
            printf("stack: 0x%lx\n", stack);
            *stack = argv_len;
            printf("argv_len: %lu\n", argv_len);
            printf("pushed: %lu\n", *stack);
            //*(--stack) = (uint64_t)stack;
            //new_task->regs.rdi = (uint64_t)stack;
            new_task->regs.rsp = (uint64_t)stack;
            //new_task->regs.rsp -= (uint64_t)stack_top - (uint64_t)stack;
            printf("rsp: 0x%lx\n", new_task->regs.rsp);
        }
    } else {
        new_task->stack_addr = ((void *)pmm_alloc(STACK_SIZE/4096));
        new_task->regs.rsp = ((uint64_t)new_task->stack_addr)+STACK_SIZE+VMM_HIGHER_HALF;
    }
    memset(new_task->stack_addr, 0, STACK_SIZE);
    task_t *task_p = root_task;
    do {
        task_p = task_p->next;
    } while(task_p->last_task != true); 
    task_p->last_task = false;
    new_task->state = TASK_READY;
    task_p->next = new_task; 
    new_task->last_task = true;
    new_task->pgm = pgm;
    new_task->mmap_anon_base = 0x80000000000;
    mgmt_on_new_program(new_task->pid);
    spinlock_release(&sched_lock);
    _sched_stop_internal = false;
    //log.debug("Task with name %s created, entrypoint: 0x%lx, usermode: %d, pagemap: 0x%lx, pid: %d\n", name, task, usermode, pgm, new_task->pid);
}
