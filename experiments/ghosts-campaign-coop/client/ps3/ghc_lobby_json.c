#include "ghc_lobby_json.h"

#include "ghc_lobby_string.h"

static int gh_is_space(char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

static const char *gh_skip_space(const char *value) {
    while (*value && gh_is_space(*value)) ++value;
    return value;
}

static int gh_hex_value(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

/* Decode JSON strings used by the lobby contract. Unicode escapes are rejected
 * deliberately: all protocol identifiers and room fields are ASCII. */
static int gh_parse_json_string(
    const char *input,
    char *output,
    size_t output_capacity,
    const char **end
) {
    size_t length = 0;
    const char *cursor;

    if (!input || *input != '"' || !output || output_capacity == 0 || !end) return -1;
    cursor = input + 1;

    while (*cursor && *cursor != '"') {
        unsigned char value = (unsigned char)*cursor++;
        if (value == '\\') {
            char escaped = *cursor++;
            if (!escaped) return -1;
            switch (escaped) {
                case '"': value = '"'; break;
                case '\\': value = '\\'; break;
                case '/': value = '/'; break;
                case 'b': value = '\b'; break;
                case 'f': value = '\f'; break;
                case 'n': value = '\n'; break;
                case 'r': value = '\r'; break;
                case 't': value = '\t'; break;
                default: return -1;
            }
        } else if (value < 0x20u) {
            return -1;
        }

        if (length + 1 >= output_capacity) return -2;
        output[length++] = (char)value;
    }

    if (*cursor != '"') return -1;
    output[length] = '\0';
    *end = cursor + 1;
    return 0;
}

int ghc_json_get_string(
    const char *json,
    const char *key,
    char *output,
    size_t output_capacity
) {
    const char *cursor;
    size_t key_length;
    char parsed_key[96];

    if (!json || !key || !output || output_capacity == 0) return -1;
    key_length = ghc_strlen(key);
    if (key_length == 0 || key_length >= sizeof(parsed_key)) return -1;

    cursor = json;
    while (*cursor) {
        if (*cursor != '"') {
            ++cursor;
            continue;
        }

        {
            const char *after_key = NULL;
            const char *value_start;
            int parse_result = gh_parse_json_string(cursor, parsed_key, sizeof(parsed_key), &after_key);
            if (parse_result != 0) return -1;

            value_start = gh_skip_space(after_key);
            if (*value_start == ':') {
                value_start = gh_skip_space(value_start + 1);
                if (ghc_strlen(parsed_key) == key_length
                    && ghc_memcmp(parsed_key, key, key_length) == 0) {
                    const char *after_value = NULL;
                    return gh_parse_json_string(value_start, output, output_capacity, &after_value);
                }
            }
            cursor = after_key;
        }
    }
    return -1;
}

int ghc_json_get_u32(const char *json, const char *key, uint32_t *value) {
    const char *cursor;
    char parsed_key[96];
    size_t key_length;

    if (!json || !key || !value) return -1;
    key_length = ghc_strlen(key);
    if (key_length == 0 || key_length >= sizeof(parsed_key)) return -1;

    cursor = json;
    while (*cursor) {
        if (*cursor != '"') {
            ++cursor;
            continue;
        }

        {
            const char *after_key = NULL;
            const char *number;
            int parse_result = gh_parse_json_string(cursor, parsed_key, sizeof(parsed_key), &after_key);
            if (parse_result != 0) return -1;

            number = gh_skip_space(after_key);
            if (*number == ':'
                && ghc_strlen(parsed_key) == key_length
                && ghc_memcmp(parsed_key, key, key_length) == 0) {
                uint32_t parsed = 0;
                number = gh_skip_space(number + 1);
                if (*number < '0' || *number > '9') return -1;
                while (*number >= '0' && *number <= '9') {
                    uint32_t digit = (uint32_t)(*number - '0');
                    if (parsed > (UINT32_MAX - digit) / 10u) return -1;
                    parsed = parsed * 10u + digit;
                    ++number;
                }
                if (*gh_skip_space(number) == '.' || *gh_skip_space(number) == 'e'
                    || *gh_skip_space(number) == 'E' || *gh_skip_space(number) == '-') return -1;
                *value = parsed;
                return 0;
            }
            cursor = after_key;
        }
    }
    return -1;
}

int ghc_hex_decode_exact(const char *hex, uint8_t *output, size_t output_length) {
    size_t i;
    if (!hex || !output || ghc_strlen(hex) != output_length * 2u) return -1;
    for (i = 0; i < output_length; ++i) {
        int high = gh_hex_value(hex[i * 2u]);
        int low = gh_hex_value(hex[i * 2u + 1u]);
        if (high < 0 || low < 0) return -1;
        output[i] = (uint8_t)((high << 4) | low);
    }
    return 0;
}

int ghc_lobby_parse_guest_session(const char *json, GhcGuestSession *session) {
    char relay_id_hex[33];
    char relay_key_hex[65];
    uint8_t token_bytes[32];
    uint8_t session_id_bytes[16];
    if (!json || !session) return -1;

    ghc_memset(session, 0, sizeof(*session));
    if (ghc_json_get_string(json, "token", session->token, sizeof(session->token)) != 0
        || ghc_json_get_string(json, "session_id", session->session_id, sizeof(session->session_id)) != 0
        || ghc_json_get_string(json, "relay_id", relay_id_hex, sizeof(relay_id_hex)) != 0
        || ghc_json_get_string(json, "relay_key", relay_key_hex, sizeof(relay_key_hex)) != 0
        || ghc_json_get_u32(json, "expires_in_seconds", &session->expires_in_seconds) != 0) {
        ghc_memset(session, 0, sizeof(*session));
        return -1;
    }

    if (ghc_strlen(session->token) != GHC_LOBBY_TOKEN_HEX_LENGTH
        || ghc_strlen(session->session_id) != GHC_LOBBY_SESSION_ID_HEX_LENGTH
        || ghc_hex_decode_exact(session->token, token_bytes, sizeof(token_bytes)) != 0
        || ghc_hex_decode_exact(session->session_id, session_id_bytes, sizeof(session_id_bytes)) != 0
        || ghc_hex_decode_exact(relay_id_hex, session->relay_id, sizeof(session->relay_id)) != 0
        || ghc_hex_decode_exact(relay_key_hex, session->relay_key, sizeof(session->relay_key)) != 0) {
        ghc_memset(session, 0, sizeof(*session));
        return -1;
    }
    return 0;
}

int ghc_json_get_bool(const char *json, const char *key, int *value) {
    const char *cursor;
    char parsed_key[96];
    size_t key_length;

    if (!json || !key || !value) return -1;
    key_length = ghc_strlen(key);
    if (key_length == 0 || key_length >= sizeof(parsed_key)) return -1;

    cursor = json;
    while (*cursor) {
        if (*cursor != '"') {
            ++cursor;
            continue;
        }

        {
            const char *after_key = NULL;
            const char *boolean;
            int parse_result = gh_parse_json_string(cursor, parsed_key, sizeof(parsed_key), &after_key);
            if (parse_result != 0) return -1;

            boolean = gh_skip_space(after_key);
            if (*boolean == ':'
                && ghc_strlen(parsed_key) == key_length
                && ghc_memcmp(parsed_key, key, key_length) == 0) {
                boolean = gh_skip_space(boolean + 1);
                if (ghc_strncmp(boolean, "true", 4) == 0) {
                    *value = 1;
                    return 0;
                }
                if (ghc_strncmp(boolean, "false", 5) == 0) {
                    *value = 0;
                    return 0;
                }
                return -1;
            }
            cursor = after_key;
        }
    }
    return -1;
}

static int gh_lobby_is_ipv4(const char *address) {
    unsigned int octet = 0;
    unsigned int digits = 0;
    unsigned int dots = 0;
    const char *cursor = address;

    if (!address || !*address) return 0;
    while (*cursor) {
        if (*cursor >= '0' && *cursor <= '9') {
            octet = octet * 10u + (unsigned int)(*cursor - '0');
            if (octet > 255u || ++digits > 3u) return 0;
        } else if (*cursor == '.') {
            if (digits == 0 || ++dots > 3u) return 0;
            octet = 0;
            digits = 0;
        } else {
            return 0;
        }
        ++cursor;
    }
    return dots == 3u && digits > 0;
}

int ghc_lobby_parse_peer_snapshot(const char *json, GhcPeerSnapshot *snapshot) {
    char room_status[16];
    char relay_id_hex[33];
    char relay_key_hex[65];
    char peer_session_id[GHC_LOBBY_SESSION_ID_HEX_LENGTH + 1];
    char public_address[16];
    char local_address[16];
    char nat_type[16];
    uint32_t slot = 0;
    uint32_t public_port = 0;
    uint32_t local_port = 0;
    int peer_ready = 0;
    int has_peer_candidate = 1;
    uint8_t session_id_bytes[16];

    if (!json || !snapshot) return -1;
    ghc_memset(snapshot, 0, sizeof(*snapshot));

    if (ghc_json_get_string(json, "room_status", room_status, sizeof(room_status)) != 0) return -1;
    if (ghc_strncmp(room_status, "running", sizeof(room_status)) == 0) {
        snapshot->room_running = 1;
    } else if (ghc_strncmp(room_status, "waiting", sizeof(room_status)) != 0) {
        return -1;
    }

    if (ghc_json_get_bool(json, "peer_ready", &peer_ready) == 0) {
        snapshot->peer_ready = (uint8_t)(peer_ready != 0);
    }

    if (ghc_json_get_string(json, "session_id", peer_session_id, sizeof(peer_session_id)) != 0) {
        return 0; /* peer is null / not joined yet */
    }
    if (ghc_json_get_u32(json, "slot", &slot) != 0 || (slot != 1u && slot != 2u)) return -1;
    if (ghc_json_get_string(json, "relay_id", relay_id_hex, sizeof(relay_id_hex)) != 0
        || ghc_json_get_string(json, "relay_key", relay_key_hex, sizeof(relay_key_hex)) != 0
        || ghc_hex_decode_exact(peer_session_id, session_id_bytes, sizeof(session_id_bytes)) != 0
        || ghc_hex_decode_exact(relay_id_hex, snapshot->relay_id, sizeof(snapshot->relay_id)) != 0
        || ghc_hex_decode_exact(relay_key_hex, snapshot->relay_key, sizeof(snapshot->relay_key)) != 0) {
        ghc_memset(snapshot, 0, sizeof(*snapshot));
        return -1;
    }

    ghc_memcpy(snapshot->peer_session_id, peer_session_id, sizeof(peer_session_id));
    snapshot->peer_slot = (uint8_t)slot;
    snapshot->peer_present = 1;

    if (ghc_json_get_string(json, "public_address", public_address, sizeof(public_address)) != 0
        || ghc_json_get_u32(json, "public_port", &public_port) != 0
        || ghc_json_get_string(json, "local_address", local_address, sizeof(local_address)) != 0
        || ghc_json_get_u32(json, "local_port", &local_port) != 0
        || ghc_json_get_string(json, "nat_type", nat_type, sizeof(nat_type)) != 0) {
        has_peer_candidate = 0;
    }

    if (has_peer_candidate) {
        if (!gh_lobby_is_ipv4(public_address) || !gh_lobby_is_ipv4(local_address)
            || public_port == 0 || public_port > 65535u
            || local_port == 0 || local_port > 65535u
            || !(ghc_strncmp(nat_type, "open", 16) == 0
                 || ghc_strncmp(nat_type, "moderate", 16) == 0
                 || ghc_strncmp(nat_type, "strict", 16) == 0
                 || ghc_strncmp(nat_type, "symmetric", 16) == 0
                 || ghc_strncmp(nat_type, "unknown", 16) == 0)) {
            ghc_memset(snapshot, 0, sizeof(*snapshot));
            return -1;
        }
        ghc_memcpy(snapshot->candidate.public_address, public_address, ghc_strlen(public_address) + 1u);
        ghc_memcpy(snapshot->candidate.local_address, local_address, ghc_strlen(local_address) + 1u);
        ghc_memcpy(snapshot->candidate.nat_type, nat_type, ghc_strlen(nat_type) + 1u);
        snapshot->candidate.public_port = (uint16_t)public_port;
        snapshot->candidate.local_port = (uint16_t)local_port;
        snapshot->candidate_present = 1;
    }
    return 0;
}

int ghc_lobby_parse_room_code(const char *json, char room_code[7]) {
    size_t i;
    if (!json || !room_code) return -1;
    if (ghc_json_get_string(json, "code", room_code, 7) != 0) return -1;
    if (ghc_strlen(room_code) != GHC_LOBBY_ROOM_CODE_LENGTH) return -1;
    for (i = 0; i < GHC_LOBBY_ROOM_CODE_LENGTH; ++i) {
        char c = room_code[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            room_code[0] = '\0';
            return -1;
        }
    }
    return 0;
}
