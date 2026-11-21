#include "../client/ps3/protocol.h"

_Static_assert(GHC_CLIENT_HEADER_SIZE == 24, "client header mismatch");
_Static_assert(GHC_SERVER_HEADER_SIZE == 9, "server header mismatch");
_Static_assert(GHC_CLIENT_OVERHEAD == 40, "client overhead mismatch");
_Static_assert(GHC_SERVER_OVERHEAD == 25, "server overhead mismatch");
_Static_assert(GHC_MAX_DATAGRAM == 1200, "MTU mismatch");

int main(void) {
    return GHC_DEFAULT_UDP_PORT == 28991 ? 0 : 1;
}
