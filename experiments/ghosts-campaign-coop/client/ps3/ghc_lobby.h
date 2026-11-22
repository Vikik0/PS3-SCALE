#pragma once
#include "ghc_lobby_json.h"

#define GHC_LOBBY_OK 0
#define GHC_LOBBY_ERR_NOT_INITIALIZED -1000
#define GHC_LOBBY_ERR_BAD_URL -1001
#define GHC_LOBBY_ERR_RESPONSE_TOO_LARGE -1002
#define GHC_LOBBY_ERR_BAD_RESPONSE -1003
#define GHC_LOBBY_ERR_HTTP_STATUS -1004

/*
 * Initializes the PS3 network / SSL / HTTPS / HTTP modules.
 * Call from a dedicated worker thread, not from a render or game hook.
 */
int ghc_lobby_runtime_start(void);
void ghc_lobby_runtime_stop(void);

/* Guest mode is independent of PSN. This does not bypass PSN bans. */
int ghc_lobby_create_guest_session(
    const char *https_base_url,
    GhcGuestSession *session,
    int *http_status
);

int ghc_lobby_create_room(
    const char *https_base_url,
    const GhcGuestSession *session,
    char room_code[7],
    int *http_status
);

int ghc_lobby_join_room(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    int *http_status
);

int ghc_lobby_set_ready(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    int ready,
    int *http_status
);

int ghc_lobby_start_mission(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    int *http_status
);

int ghc_lobby_leave_room(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    int *http_status
);


int ghc_lobby_get_peer(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    GhcPeerSnapshot *snapshot,
    int *http_status
);

int ghc_lobby_publish_candidate(
    const char *https_base_url,
    const GhcGuestSession *session,
    const char room_code[7],
    const GhcNetworkCandidate *candidate,
    GhcPeerSnapshot *snapshot,
    int *http_status
);
