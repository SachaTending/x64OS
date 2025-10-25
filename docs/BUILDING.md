# Requirements
To build a kernel, you need: make, git, gcc, wget, curl and xorriso

# Building
To build an iso image
```bash
make template-ARCH.iso
```

As a result of building, you get template-ARCH.iso image, which is a bootable uefi and csm iso image with limine bootloader

To build an raw hdd image

```bash
make template-ARCH.hdd
```

As a result of building, you get template-ARCH.hdd image, which is a bootable uefi and csm raw hard drive image with limine bootloader preinstalled

ARCH is an architecture that you are targeting. Currently kernel supports only x86_64.