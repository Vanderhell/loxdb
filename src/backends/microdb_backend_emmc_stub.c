#include <stdint.h>

const char *microdb_backend_emmc_stub_id(void) {
    return "microdb_backend_emmc_stub";
}

int microdb_backend_emmc_stub_marker(void) {
    return (int)(uint8_t)0x45u;
}
