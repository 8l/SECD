#ifndef __SECD_DEBUGH__
#define __SECD_DEBUGH__

#include "conf.h"

#include <stdio.h>

#if (MEMDEBUG)
# define memdebugf(...) do { \
    printf("%ld |   ", secd->tick); \
    printf(__VA_ARGS__);  \
  } while (0)
# if (MEMTRACE)
#  define memtracef(...) printf(__VA_ARGS__)
# else
#  define memtracef(...)
# endif
#else
# define memdebugf(...)
# define memtracef(...)
#endif

#if (CTRLDEBUG)
# define ctrldebugf(...) do { \
    printf("%ld | ", secd->tick); \
    printf(__VA_ARGS__);  \
  } while (0)
#else
# define ctrldebugf(...)
#endif

#if (ENVDEBUG)
# define envdebugf(...) printf(__VA_ARGS__)
#else
# define envdebugf(...)
#endif

#ifndef __unused
# define __unused __attribute__((unused))
#endif


#endif //__SECD_DEBUGH__
