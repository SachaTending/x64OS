#pragma once
#include <fs/resource.h>

void partition_enum(struct resource *root, const char *rootname, uint16_t blocksize, const char *convention);