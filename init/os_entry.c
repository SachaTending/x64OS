__attribute__((noreturn)) void _start2(void) {
    register int    syscall_no  asm("rax") = 1;
    register int    arg1        asm("rdi") = 1;
    register char*  arg2        asm("rsi") = "this is not executable file, its a kernel, k e r n e l, not simple elf file, boot it via limine to see what it does. btw this is a x64OS kernel.\n";
    register int    arg3        asm("rdx") = 146;
    syscall_no = 1;
    asm("syscall");
    syscall_no = 60;
    arg1 = 0;
    asm("syscall");
    (void)syscall_no;
    (void)arg1;
    (void)arg2;
    (void)arg3;
    while(1);
}
