#include <stdint.h>

const char *microdb_backend_sd_stub_id(void) {
    return "microdb_backend_sd_stub";
}

int microdb_backend_sd_stub_marker(void) {
    return (int)(uint8_t)0x53u;
}
