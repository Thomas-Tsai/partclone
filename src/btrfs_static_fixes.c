#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(HAVE_XXHASH)
/* Scenario: xxhash disabled. We provide implementation (unprefixed) for btrfs. */
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "btrfs/crypto/xxhash.h"
#elif defined(BTRFS_STATIC_BUILD)
/* Scenario: static build + enabled. We provide wrapped implementation to avoid conflict. */
#define XXH_NAMESPACE BTRFS_
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "btrfs/crypto/xxhash.h"
#endif