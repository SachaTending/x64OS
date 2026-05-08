#include <elf.h>
#include <vfs.hpp>
#include <libc.h>
#include <mmap.h>
#include <sched/sched.hpp>
#include <prg_loading.hpp>
#include <logging.hpp>

static Logger log("elf loader");

bool elf_load(struct pagemap *pagemap, struct resource *res, uint64_t load_base,
              struct auxval *auxv, const char **ld_path) {
    Elf64_Ehdr header;
    if (res->read(res, NULL, &header, 0, sizeof(header)) < 0) {
        log.info("Failed to load program: too small.\n");
        return false;
    }

    if (memcmp(header.e_ident, ELFMAG, 4) != 0) {
        log.info("Failed to load program: not an elf\n");
        //errno = ENOEXEC;
        return false;
    }

    if (header.e_ident[EI_CLASS] != ELFCLASS64 || header.e_ident[EI_DATA] != ELFDATA2LSB ||
        header.e_ident[EI_OSABI] != 0 /* ELFOSABI_SYSV */ || header.e_machine != EM_X86_64) {
        //errno = ENOEXEC;
        log.info("Invalid program.\n");
        return false;
    }

    for (size_t i = 0; i < header.e_phnum; i++) {
        Elf64_Phdr phdr;
        if (res->read(res, NULL, &phdr, header.e_phoff + i * header.e_phentsize, sizeof(phdr)) < 0) {
            log.info("Error while reading header: EOF\n");
            goto fail;
        }

        switch (phdr.p_type) {
            case PT_TLS:
                log.info("PT_TLS 0x%lx, size: %lu, offs: 0x%lx\n", phdr.p_vaddr, phdr.p_memsz, phdr.p_offset);
                break;
            case PT_LOAD: {
                int prot = PROT_READ;
                if (phdr.p_flags & PF_W) {
                    prot |= PROT_WRITE;
                }
                if (phdr.p_flags & PF_X) {
                    prot |= PROT_EXEC;
                }
                log.info("PT_LOAD 0x%lx, size: %lu, offset: 0x%08lx\n", phdr.p_vaddr, phdr.p_memsz, phdr.p_offset);

                size_t misalign = phdr.p_vaddr & (PAGE_SIZE - 1);
                size_t page_count = DIV_ROUNDUP(phdr.p_memsz + misalign, PAGE_SIZE);

                void *phys = pmm_alloc(page_count);
                if (phys == NULL) {
                    goto fail;
                }

                if (!mmap_range(pagemap, phdr.p_vaddr + load_base, (uintptr_t)phys,
                                page_count * PAGE_SIZE, prot, MAP_ANONYMOUS)) {
                    pmm_free(phys, page_count);
                    log.error("Failed to load: Error while mapping.\n");
                    goto fail;
                }

                if (res->read(res, NULL, phys + misalign + VMM_HIGHER_HALF, phdr.p_offset, phdr.p_filesz) < 0) {
                    log.error("Failed to load: EOF when loading PT_LOAD\n");
                    goto fail;
                }

                // TODO: Figure out why mmap-ing the file breaks stuff
                // uintptr_t virt_start = ALIGN_DOWN(phdr.p_vaddr, PAGE_SIZE);
                // uintptr_t virt_end = ALIGN_UP(phdr.p_vaddr + phdr.p_memsz, PAGE_SIZE);
                // uintptr_t virt_file_end = ALIGN_UP(phdr.p_vaddr + phdr.p_filesz, PAGE_SIZE);

                // size_t misalign = phdr.p_vaddr - virt_start;

                // if (mmap(pagemap, virt_start + load_base, phdr.p_filesz + misalign, prot,
                //          MAP_PRIVATE | MAP_FIXED, res, phdr.p_offset + misalign) == NULL) {
                //     goto fail;
                // }

                // if (virt_end > virt_file_end) {
                //     if (mmap(pagemap, virt_file_end, virt_end - virt_file_end, prot,
                //              MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, NULL, 0) == NULL) {
                //         goto fail;
                //     }
                // }

                break;
            }
            case PT_PHDR:
                auxv->at_phdr = phdr.p_vaddr + load_base;
                break;
            case PT_INTERP: {
                void *path = malloc(phdr.p_filesz + 1);
                if (path == NULL) {
                    log.error("Failed to load: Error while aloocating memory for elf string.\n");
                    goto fail;
                }

                if (res->read(res, NULL, path, phdr.p_offset, phdr.p_filesz) < 0) {
                    free(path);
                    log.error("Failed to load: EOF while reading interpreter string.\n");
                    goto fail;
                }

                if (ld_path != NULL) {
                    *ld_path = (const char *) path;
                }
                break;
            }
        }
    }

    auxv->at_entry = header.e_entry + load_base;
    auxv->at_phent = header.e_phentsize;
    auxv->at_phnum = header.e_phnum;
    return true;

fail:
    if (ld_path != NULL && *ld_path != NULL) {
        free((void *)*ld_path);
    }
    return false;
}