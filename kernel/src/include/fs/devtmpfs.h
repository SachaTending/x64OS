#pragma once
void devtmpfs_init(void);
bool devtmpfs_add_device(struct resource *device, const char *name);