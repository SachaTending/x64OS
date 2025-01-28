#include <module.hpp>
#include <frg/vector.hpp>
#include <frg/std_compat.hpp>
#include <new>
#include <logging.hpp>
#include <libc.h>

static Logger log("Modules subsystem");

typedef struct started_module {
    const char *name; // strdup(mod->name);
    void *mod_priv;
    module_t *orig_module;
} started_module_t;

typedef frg::vector<module_t *, frg::stl_allocator> mod_vec_t;
typedef frg::vector<started_module_t *, frg::stl_allocator> started_mod_vec_t;

mod_vec_t mod_vec = mod_vec_t();
started_mod_vec_t started_mod_vec = started_mod_vec_t();

void register_module(module_t *mod) {
    mod_vec.push_back(mod);
    log.debug("Registered module: %s\n", mod);
}

void start_module(const char *name) {
    // Check if module already started
    for (started_module_t *mod : started_mod_vec) {
        if (!strcmp(name, mod->name)) return;
    }
    
    for (module_t *mod : mod_vec) {
        if (!strcmp(mod->name, name)) {
            started_module_t *m2 = new started_module_t;
            void *ret = mod->start(mod);
            if (ret == NULL) {
                delete m2;
                return;
            }
            m2->mod_priv = ret;
            m2->name = strdup(mod->name);
            m2->orig_module = mod;
            started_mod_vec.push_back(m2);
            log.debug("Started module: %s\n", mod->name);
        }
    }
}

void start_modules() { // Can be only called by kernel
    for (module_t *mod : mod_vec) {
        start_module(mod->name);
    }
}