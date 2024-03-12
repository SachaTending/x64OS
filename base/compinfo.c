#ifndef __BUILD_USER
#define __BUILD_USER "unknown"
#endif

#ifndef __BUILD_HOST
#define __BUILD_HOST "unknown"
#endif


const char *compdate = __TIMESTAMP__; // timestamp of kernel
const char *compmachine = __BUILD_USER "@" __BUILD_HOST;