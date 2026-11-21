#include "cb_peer.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>

static int wait_input(CBPeer *peer, CBNetInput *input)
{
    int i;
    struct timespec pause = { 0, 1000000L };
    for (i = 0; i < 1000; i++) {
        int result = cb_peer_poll_input(peer, input);
        if (result != 0) return result;
        nanosleep(&pause, NULL);
    }
    return 0;
}

int main(void)
{
    CBPeer a, b, stranger;
    CBNetInput received;
    uint16_t a_port, b_port;

    assert(cb_peer_open(&a, 0) == 0);
    assert(cb_peer_open(&b, 0) == 0);
    assert(cb_peer_open(&stranger, 0) == 0);
    a_port = cb_peer_local_port(&a);
    b_port = cb_peer_local_port(&b);
    assert(a_port != 0 && b_port != 0 && a_port != b_port);

    assert(cb_peer_set_remote(&a, "127.0.0.1", b_port) == 0);
    assert(cb_peer_set_remote(&b, "127.0.0.1", a_port) == 0);
    assert(cb_peer_set_remote(&stranger, "127.0.0.1", b_port) == 0);
    assert(cb_peer_set_remote(&a, "not-an-ip", b_port) == -1);

    assert(cb_peer_send_input(&a, 1234, 0xA55Au) == 0);
    assert(wait_input(&b, &received) == 1);
    assert(received.frame == 1234);
    assert(received.buttons == 0xA55Au);
    assert(received.sequence == 1);

    assert(cb_peer_send_input(&b, 1235, 0x0055u) == 0);
    assert(wait_input(&a, &received) == 1);
    assert(received.frame == 1235);
    assert(received.buttons == 0x0055u);

    /* A valid packet from a different UDP endpoint must not be accepted. */
    assert(cb_peer_send_input(&stranger, 9999, 0xFFFFu) == 0);
    {
        struct timespec pause = { 0, 10000000L };
        nanosleep(&pause, NULL);
    }
    assert(cb_peer_poll_input(&b, &received) == 0);

    cb_peer_close(&stranger);
    cb_peer_close(&b);
    cb_peer_close(&a);
    puts("UDP peer transport tests passed.");
    return 0;
}
