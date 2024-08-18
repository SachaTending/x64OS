#include <elf.h>
#include <prg_loader.hpp>
#include <libc.h>

LOADER_ERROR elf_check_prg(struct prg_loader *prg_loader, vfs_node_t *node) {
    node->seek(node, 0, SEEK_SET); // Seek to start
    LOADER_ERROR err = LOADER_OK;
    Elf64_Ehdr *hdr = new Elf64_Ehdr; // Allocate memory for elf header
    if (hdr == NULL) { // Is memory for header allocated?
        err = LOADER_ALLOC_FAIL; // Return error alloc failed
        goto final_no_alloc;
    }
    node->read(node, (void *)hdr, sizeof(Elf64_Ehdr));
    if (memcmp((const void *)&(hdr->e_ident), (const void *)&ELFMAG, SELFMAG) || // Does file has valid header?
        hdr->e_ident[EI_DATA] != ELFDATA2LSB || // Is file little endian?
        hdr->e_ident[EI_CLASS] != ELFCLASS64 || // Is file 64 bit?
        hdr->e_type != ET_EXEC || // Is file is executable?
        hdr->e_shnum < 0 // Does file has any sections?
        ) {
        err =  LOADER_INVALID_EXEC; // Return error invalid file
        goto final;
    }
    if (hdr->e_machine != EM_X86_64) { // Check if executable is x86_64
        err = LOADER_INVALID_MACHINE; // Return error invalid machine
        goto final;
    }
    final:
    delete hdr; // Deallocate memory for header
    final_no_alloc:
    return err; // Return error(or ok, who knows?)
}

prg_loader_t elf_loader = {
    .check = elf_check_prg,
    .loader_name = "elf"
};