#pragma once
#include <sched/sched.hpp>
#include <arch/vmm.h>
#include <fs/resource.h>

bool elf_load(struct pagemap *pagemap, struct resource *res, uint64_t load_base,
              struct auxval *auxv, const char **ld_path);