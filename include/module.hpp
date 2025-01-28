#pragma once

typedef struct module
{
    const char *name;
    void *priv;
    void *(*start)(struct module *mod);
    void (*stop)(struct module *mod, void *priv);
} module_t;

void register_module(module_t *mod);