#ifndef CB_RELAY_CLIENT_H
#define CB_RELAY_CLIENT_H

#include "ws_client.h"

#include <stddef.h>
#include <stdint.h>

#define CB_RELAY_MAX_PACKET 1200
#define CB_RELAY_MAX_BATCH_PACKETS 32
#define CB_RELAY_MAX_MESSAGE 24576

typedef struct CbRelayClient {
    CbWsClient websocket;
    char room_code[7];
    char session_id[33];
    char host_session_id[33];
    char host[256];
    unsigned short port;
    uint64_t last_ping_ms;
    int connected;
} CbRelayClient;

typedef struct CbRelayPacket {
    /* Sixteen raw bytes. Set to all zeroes to broadcast to the room. */
    uint8_t recipient_id[16];
    const void *payload;
    size_t length;
} CbRelayPacket;

/* Creates a PS3 guest slot using the room code, then consumes its one-time WS ticket. */
int cb_relay_join(CbRelayClient *client, const char *host, unsigned short port,
                  const char *room_code, const char *display_name, int timeout_ms);

/* Broadcasts one CBN1 datagram to every other room member. */
int cb_relay_send_packet(CbRelayClient *client, const void *packet, size_t length);

/* Sends one datagram to a specific session ID; a NULL recipient broadcasts. */
int cb_relay_send_to(CbRelayClient *client, const uint8_t recipient_id[16],
                     const void *packet, size_t length);

/* Sends up to 32 targeted datagrams in one CBU1 message to stay within relay rate limits. */
int cb_relay_send_batch(CbRelayClient *client, const CbRelayPacket *packets, size_t count);

/* Returns 1 for a game packet, 2 for a text control message, 0 for timeout/PONG, -1 on error. */
int cb_relay_receive_packet(CbRelayClient *client, void *packet, size_t capacity,
                            size_t *length, uint8_t sender_id[16],
                            char *control, size_t control_capacity,
                            int timeout_ms);

/* Returns a stable synthetic IPv4/port for the room host, for legacy netcode address matching. */
int cb_relay_host_address(const CbRelayClient *client, char *address,
                          size_t address_capacity, unsigned short *port);

void cb_relay_leave(CbRelayClient *client);

#endif
