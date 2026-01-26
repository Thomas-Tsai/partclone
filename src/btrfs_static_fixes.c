#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(BTRFS_STATIC_BUILD)
/* For static builds, always use prefix to avoid any conflict with system libs */
#define XXH_NAMESPACE BTRFS_
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "btrfs/crypto/xxhash.h"
#elif !defined(HAVE_XXHASH)
/* Scenario: xxhash disabled and NOT a static build. We provide implementation (unprefixed) for btrfs. */
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "btrfs/crypto/xxhash.h"
#endif