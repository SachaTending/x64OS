# x64OS

*its uefi compatible*

Another attempt to write os using c++, now using limine boot protocol, sorry to risc-v and arm users, this os targets only amd64 based cpus, idk if im gotta support these cpus in future

TODO List:
 - [x] GDT
 - [x] IDT
 - [x] PMM
 - [x] VMM
 - [x] Interrupts, timers
 - [x] Serial port
 - [x] Scheduler
 - [ ] VFS
    - [ ] tmpfs
    - [ ] ext2/3/4
    - [ ] fat32
    - [ ] tarfs(basicly initrd)
 - [ ] Program loading
 - [ ] Drivers support
 - [ ] Networking
    - [ ] Ethernet
    - [ ] UDP
    - [ ] TCP
    - [ ] ICMP
    - [ ] more protocols support
 - [ ] USB


# Building and running
You need xorriso(for creating iso image), gcc(or clang, with amd64 instruction set support), nasm and make(reason why Makefile exists) 

To build, use:
```sh
make # single core building
make -j$(nproc) # or multi threaded building
```

As a result, you get:
   - bin/kernel.elf: Kernel
   - image.iso: Bootable iso image, BIOS/UEFI(x32 and x64) compatible