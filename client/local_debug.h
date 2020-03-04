#ifndef _LOCAL_DEBUG
#define _LOCAL_DEBUG

#include <stdio.h>

#ifdef APP_E

#define TRACE_APP_E(f, m...) { \
    fprintf(stderr, "APP_E: [%s:%d] " f, \
            __FILE__, __LINE__, ##m);   \
    }

#else

#define TRACE_APP_E(f, m...)    (void)0

#endif /* APP_E */

#ifdef APP_I

#define TRACE_APP_I(f, m...) { \
    fprintf(stderr, "APP_I: [%s:%d] " f, \
            __FILE__, __LINE__, ##m);   \
    }

#else

#define TRACE_APP_I(f, m...)    (void)0

#endif /* APP_I */

#ifdef APP_D

#define TRACE_APP_D(f, m...) { \
    fprintf(stderr, "APP_D: [%s:%d] " f, \
            __FILE__, __LINE__, ##m);   \
    }

#else

#define TRACE_APP_D(f, m...)    (void)0

#endif /* APP_D */

#endif /* _LOCAL_DEBUG */