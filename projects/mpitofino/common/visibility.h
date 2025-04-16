#ifndef __COMMON_VISIBILITY_H
#define __COMMON_VISIBILITY_H

/* Export a function to users of the library archive/shared object */
#define SHLIB_EXPORTED __attribute__((__visibility__("default")))

#endif /* __COMMON_VISIBILITY_H */
