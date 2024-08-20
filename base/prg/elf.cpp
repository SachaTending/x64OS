/**
 * @file base/prg/elf.cpp
 * @author TendingStream73
 * @details ELF Program loader
 * @date 18.08.2024 (dd.mm.yyyy)
 */
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

LOADER_ERROR elf_load_prg(prg_loader_t *prg_loader, vfs_node_t *node, pagemap *pgm) {
    // Check if program valid
    LOADER_ERROR err = prg_loader->check(prg_loader, node);
    if (err != LOADER_OK) { // If got error
        return err; // Return it
    }
    node->seek(node, 0, SEEK_SET); // Seek to 0
    Elf64_Ehdr *hdr = new Elf64_Ehdr; // Allocate memory for elf header
    if (hdr == NULL) { // Is memory for elf header allocated?
        return LOADER_ALLOC_FAIL; // Return error alloc failed
    }
    node->read(node, (void *)hdr, sizeof(Elf64_Ehdr)); // Read header
    node->seek(node, hdr->e_phoff, SEEK_SET);
    Elf64_Phdr *phdr = new Elf64_Phdr;
    for (int i=0;i<hdr->e_phnum;i++) {
        memset(phdr, 0, sizeof(Elf64_Phdr));
        node->read(node, phdr, sizeof(Elf64_Phdr));
        printf("phdr %i: type: %u, offset: %lu, vaddr: 0x%lx, paddr: 0x%lx, memsz: %u\n", phdr->p_type, phdr->p_offset, phdr->p_vaddr, phdr->p_paddr, phdr->p_memsz);
    }
    return LOADER_OK;
}
extern prg_loader_t elf_loader;
void init_elf() {
    register_loader(&elf_loader);
}

prg_loader_t elf_loader = {
    .magic = ELFMAG,
    .magic_len = 4,
    .loader_name = "ELF Loader",
    .check = elf_check_prg,
    .load = elf_load_prg,
    .priv = NULL
};