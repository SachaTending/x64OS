#pragma once
#include <limine.h>

extern limine_hhdm_request hhdm2;

#ifdef VMM_HIGHER_HALF
#undef VMM_HIGHER_HALF
#endif

#define VMM_HIGHER_HALF hhdm2.response->offset

namespace Kernel
{
    void Main();
} // namespace Kernel
