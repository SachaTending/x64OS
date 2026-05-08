#ifndef __BUILD_USER
#define __BUILD_USER "unknown"
#endif

#ifndef __BUILD_HOST
#define __BUILD_HOST "unknown"
#endif

#ifndef __GIT_HASH
#define __GIT_HASH "unknown"
#endif

#ifndef __BUILD_DATE
#define __BUILD_DATE "unknown"
#endif

const char *build_host = __BUILD_HOST;
const char *build_user = __BUILD_USER;
const char *git_hash = __GIT_HASH;
const char *version = "v0.1 alpha";

const char *full_ver = "x64OS-Rewrite v0.1 alpha-" __GIT_HASH " " __BUILD_USER "@" __BUILD_HOST " " __BUILD_DATE;