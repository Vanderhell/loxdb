#include "microdb_internal.h"

/* WAL is not implemented yet. The core keeps the hook point and current build
 * stays usable in RAM-only mode and for non-persistent KV testing. */
