#ifndef __BUILD_USER
#define __BUILD_USER "unknown"
#endif

#ifndef __BUILD_HOST
#define __BUILD_HOST "unknown"
#endif

#ifndef __BUILD_TIMESTAMP
#define __BUILD_TIMESTAMP __DATE__ " " __TIME__
#endif

const char *compdate = __BUILD_TIMESTAMP; // timestamp of kernel
const char *compmachine = __BUILD_USER "@" __BUILD_HOST;