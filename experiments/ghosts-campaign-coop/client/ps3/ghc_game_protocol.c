#include "ghc_game_protocol.h"

static void gh_write_u16(uint8_t *output, uint16_t value) {
    output[0] = (uint8_t)(value >> 8);
    output[1] = (uint8_t)value;
}

static void gh_write_u32(uint8_t *output, uint32_t value) {
    output[0] = (uint8_t)(value >> 24);
    output[1] = (uint8_t)(value >> 16);
    output[2] = (uint8_t)(value >> 8);
    output[3] = (uint8_t)value;
}

static uint16_t gh_read_u16(const uint8_t *input) {
    return (uint16_t)(((uint16_t)input[0] << 8) | input[1]);
}

static uint32_t gh_read_u32(const uint8_t *input) {
    return ((uint32_t)input[0] << 24)
        | ((uint32_t)input[1] << 16)
        | ((uint32_t)input[2] << 8)
        | (uint32_t)input[3];
}

static int32_t gh_read_i32(const uint8_t *input) {
    return (int32_t)gh_read_u32(input);
}

static int16_t gh_read_i16(const uint8_t *input) {
    return (int16_t)gh_read_u16(input);
}

static void gh_write_i32(uint8_t *output, int32_t value) {
    gh_write_u32(output, (uint32_t)value);
}

static void gh_write_i16(uint8_t *output, int16_t value) {
    gh_write_u16(output, (uint16_t)value);
}

static int gh_valid_slot(uint8_t slot) {
    return slot == 1u || slot == 2u;
}

static int gh_valid_player_state(const GhcGamePlayerState *state) {
    const uint16_t known_flags = GHC_MOVE_FORWARD | GHC_MOVE_BACKWARD | GHC_MOVE_LEFT
        | GHC_MOVE_RIGHT | GHC_MOVE_JUMP | GHC_MOVE_CROUCH | GHC_MOVE_SPRINT
        | GHC_MOVE_AIM | GHC_MOVE_FIRE;
    if (!gh_valid_slot(state->slot) || state->stance > GHC_STANCE_DOWNED
        || state->yaw_centidegrees >= 36000u
        || state->pitch_centidegrees < -9000 || state->pitch_centidegrees > 9000
        || (state->movement_flags & (uint16_t)~known_flags) != 0
        || state->health > 100u || state->armor > 100u) return 0;
    return 1;
}

static int gh_valid_message(const GhcGameMessage *message) {
    switch (message->type) {
        case GHC_GAME_PLAYER_STATE:
            return gh_valid_player_state(&message->data.player_state);
        case GHC_GAME_PLAYER_ACTION:
            return gh_valid_slot(message->data.player_action.slot)
                && message->data.player_action.action >= GHC_ACTION_FIRE
                && message->data.player_action.action <= GHC_ACTION_GRENADE;
        case GHC_GAME_DAMAGE_EVENT:
            return (message->data.damage_event.attacker_slot <= 2u)
                && gh_valid_slot(message->data.damage_event.victim_slot)
                && message->data.damage_event.damage <= 1000u
                && message->data.damage_event.resulting_health <= 100u;
        case GHC_GAME_REVIVE_EVENT:
            return gh_valid_slot(message->data.revive_event.reviver_slot)
                && gh_valid_slot(message->data.revive_event.downed_slot)
                && message->data.revive_event.reviver_slot != message->data.revive_event.downed_slot
                && message->data.revive_event.phase >= GHC_REVIVE_START
                && message->data.revive_event.phase <= GHC_REVIVE_FAILED;
        case GHC_GAME_MISSION_EVENT:
            return message->data.mission_event.event_type >= GHC_MISSION_CHECKPOINT_REACHED
                && message->data.mission_event.event_type <= GHC_MISSION_RESTART;
        case GHC_GAME_HEARTBEAT:
            return gh_valid_slot(message->data.heartbeat.slot);
        default:
            return 0;
    }
}

static size_t gh_payload_size(uint8_t type) {
    switch (type) {
        case GHC_GAME_PLAYER_STATE: return 40u;
        case GHC_GAME_PLAYER_ACTION: return 14u;
        case GHC_GAME_DAMAGE_EVENT: return 16u;
        case GHC_GAME_REVIVE_EVENT: return 16u;
        case GHC_GAME_MISSION_EVENT: return 14u;
        case GHC_GAME_HEARTBEAT: return 8u;
        default: return 0;
    }
}

static void gh_encode_payload(const GhcGameMessage *message, uint8_t *output) {
    size_t cursor = 0;
    size_t i;
    switch (message->type) {
        case GHC_GAME_PLAYER_STATE: {
            const GhcGamePlayerState *s = &message->data.player_state;
            output[cursor++] = s->slot;
            gh_write_u16(output + cursor, s->actor_id); cursor += 2;
            gh_write_u32(output + cursor, s->game_tick); cursor += 4;
            for (i = 0; i < 3; ++i) { gh_write_i32(output + cursor, s->position_cm[i]); cursor += 4; }
            for (i = 0; i < 3; ++i) { gh_write_i16(output + cursor, s->velocity_cms[i]); cursor += 2; }
            gh_write_u16(output + cursor, s->yaw_centidegrees); cursor += 2;
            gh_write_i16(output + cursor, s->pitch_centidegrees); cursor += 2;
            gh_write_u16(output + cursor, s->movement_flags); cursor += 2;
            output[cursor++] = s->stance;
            gh_write_u16(output + cursor, s->weapon_id); cursor += 2;
            gh_write_u16(output + cursor, s->ammo_clip); cursor += 2;
            gh_write_u16(output + cursor, s->ammo_reserve); cursor += 2;
            output[cursor++] = s->health;
            output[cursor++] = s->armor;
            break;
        }
        case GHC_GAME_PLAYER_ACTION: {
            const GhcGamePlayerAction *a = &message->data.player_action;
            output[cursor++] = a->slot;
            output[cursor++] = a->action;
            gh_write_u16(output + cursor, a->actor_id); cursor += 2;
            gh_write_u16(output + cursor, a->target_actor_id); cursor += 2;
            gh_write_u16(output + cursor, a->weapon_id); cursor += 2;
            gh_write_u16(output + cursor, a->value); cursor += 2;
            gh_write_u32(output + cursor, a->game_tick); cursor += 4;
            break;
        }
        case GHC_GAME_DAMAGE_EVENT: {
            const GhcGameDamageEvent *d = &message->data.damage_event;
            gh_write_u32(output + cursor, d->event_id); cursor += 4;
            output[cursor++] = d->attacker_slot;
            output[cursor++] = d->victim_slot;
            gh_write_u16(output + cursor, d->damage); cursor += 2;
            output[cursor++] = d->damage_type;
            output[cursor++] = d->hit_location;
            output[cursor++] = d->resulting_health;
            output[cursor++] = d->flags;
            gh_write_u32(output + cursor, d->game_tick); cursor += 4;
            break;
        }
        case GHC_GAME_REVIVE_EVENT: {
            const GhcGameReviveEvent *r = &message->data.revive_event;
            gh_write_u32(output + cursor, r->event_id); cursor += 4;
            output[cursor++] = r->reviver_slot;
            output[cursor++] = r->downed_slot;
            output[cursor++] = r->phase;
            gh_write_u16(output + cursor, r->progress_ms); cursor += 2;
            gh_write_u16(output + cursor, r->remaining_ms); cursor += 2;
            output[cursor++] = r->result;
            gh_write_u32(output + cursor, r->game_tick); cursor += 4;
            break;
        }
        case GHC_GAME_MISSION_EVENT: {
            const GhcGameMissionEvent *m = &message->data.mission_event;
            gh_write_u32(output + cursor, m->event_id); cursor += 4;
            output[cursor++] = m->event_type;
            output[cursor++] = m->state;
            gh_write_u16(output + cursor, m->checkpoint_id); cursor += 2;
            gh_write_u16(output + cursor, m->objective_id); cursor += 2;
            gh_write_u32(output + cursor, m->game_tick); cursor += 4;
            break;
        }
        case GHC_GAME_HEARTBEAT: {
            const GhcGameHeartbeat *h = &message->data.heartbeat;
            gh_write_u32(output + cursor, h->game_tick); cursor += 4;
            gh_write_u16(output + cursor, h->ping_ms); cursor += 2;
            output[cursor++] = h->slot;
            output[cursor++] = h->status;
            break;
        }
    }
}

static int gh_decode_payload(uint8_t type, const uint8_t *input, GhcGameMessage *message) {
    size_t cursor = 0;
    size_t i;
    switch (type) {
        case GHC_GAME_PLAYER_STATE: {
            GhcGamePlayerState *s = &message->data.player_state;
            s->slot = input[cursor++];
            s->actor_id = gh_read_u16(input + cursor); cursor += 2;
            s->game_tick = gh_read_u32(input + cursor); cursor += 4;
            for (i = 0; i < 3; ++i) { s->position_cm[i] = gh_read_i32(input + cursor); cursor += 4; }
            for (i = 0; i < 3; ++i) { s->velocity_cms[i] = gh_read_i16(input + cursor); cursor += 2; }
            s->yaw_centidegrees = gh_read_u16(input + cursor); cursor += 2;
            s->pitch_centidegrees = gh_read_i16(input + cursor); cursor += 2;
            s->movement_flags = gh_read_u16(input + cursor); cursor += 2;
            s->stance = input[cursor++];
            s->weapon_id = gh_read_u16(input + cursor); cursor += 2;
            s->ammo_clip = gh_read_u16(input + cursor); cursor += 2;
            s->ammo_reserve = gh_read_u16(input + cursor); cursor += 2;
            s->health = input[cursor++];
            s->armor = input[cursor++];
            break;
        }
        case GHC_GAME_PLAYER_ACTION: {
            GhcGamePlayerAction *a = &message->data.player_action;
            a->slot = input[cursor++];
            a->action = input[cursor++];
            a->actor_id = gh_read_u16(input + cursor); cursor += 2;
            a->target_actor_id = gh_read_u16(input + cursor); cursor += 2;
            a->weapon_id = gh_read_u16(input + cursor); cursor += 2;
            a->value = gh_read_u16(input + cursor); cursor += 2;
            a->game_tick = gh_read_u32(input + cursor); cursor += 4;
            break;
        }
        case GHC_GAME_DAMAGE_EVENT: {
            GhcGameDamageEvent *d = &message->data.damage_event;
            d->event_id = gh_read_u32(input + cursor); cursor += 4;
            d->attacker_slot = input[cursor++];
            d->victim_slot = input[cursor++];
            d->damage = gh_read_u16(input + cursor); cursor += 2;
            d->damage_type = input[cursor++];
            d->hit_location = input[cursor++];
            d->resulting_health = input[cursor++];
            d->flags = input[cursor++];
            d->game_tick = gh_read_u32(input + cursor); cursor += 4;
            break;
        }
        case GHC_GAME_REVIVE_EVENT: {
            GhcGameReviveEvent *r = &message->data.revive_event;
            r->event_id = gh_read_u32(input + cursor); cursor += 4;
            r->reviver_slot = input[cursor++];
            r->downed_slot = input[cursor++];
            r->phase = input[cursor++];
            r->progress_ms = gh_read_u16(input + cursor); cursor += 2;
            r->remaining_ms = gh_read_u16(input + cursor); cursor += 2;
            r->result = input[cursor++];
            r->game_tick = gh_read_u32(input + cursor); cursor += 4;
            break;
        }
        case GHC_GAME_MISSION_EVENT: {
            GhcGameMissionEvent *m = &message->data.mission_event;
            m->event_id = gh_read_u32(input + cursor); cursor += 4;
            m->event_type = input[cursor++];
            m->state = input[cursor++];
            m->checkpoint_id = gh_read_u16(input + cursor); cursor += 2;
            m->objective_id = gh_read_u16(input + cursor); cursor += 2;
            m->game_tick = gh_read_u32(input + cursor); cursor += 4;
            break;
        }
        case GHC_GAME_HEARTBEAT: {
            GhcGameHeartbeat *h = &message->data.heartbeat;
            h->game_tick = gh_read_u32(input + cursor); cursor += 4;
            h->ping_ms = gh_read_u16(input + cursor); cursor += 2;
            h->slot = input[cursor++];
            h->status = input[cursor++];
            break;
        }
        default:
            return -1;
    }
    (void)cursor;
    return 0;
}

int ghc_game_encode_message(
    const GhcGameMessage *message,
    uint8_t *output,
    size_t output_capacity,
    size_t *output_length
) {
    size_t payload_size;
    if (!message || !output || !output_length) return -1;
    *output_length = 0;
    if (!gh_valid_message(message)) return -1;
    payload_size = gh_payload_size(message->type);
    if (payload_size == 0 || output_capacity < GHC_GAME_HEADER_SIZE + payload_size) return -1;

    output[0] = 'G'; output[1] = 'C'; output[2] = 'P'; output[3] = '1';
    output[4] = GHC_GAME_VERSION;
    output[5] = message->type;
    gh_write_u16(output + 6, (uint16_t)payload_size);
    gh_encode_payload(message, output + GHC_GAME_HEADER_SIZE);
    *output_length = GHC_GAME_HEADER_SIZE + payload_size;
    return 0;
}

int ghc_game_decode_message(
    const uint8_t *packet,
    size_t packet_length,
    GhcGameMessage *message
) {
    size_t payload_size;
    if (!packet || !message || packet_length < GHC_GAME_HEADER_SIZE) return -1;
    if (packet[0] != 'G' || packet[1] != 'C' || packet[2] != 'P' || packet[3] != '1') return -1;
    if (packet[4] != GHC_GAME_VERSION) return -1;

    payload_size = gh_payload_size(packet[5]);
    if (payload_size == 0 || gh_read_u16(packet + 6) != payload_size
        || packet_length != GHC_GAME_HEADER_SIZE + payload_size) return -1;

    {
        uint8_t *bytes = (uint8_t *)message;
        size_t i;
        for (i = 0; i < sizeof(*message); ++i) bytes[i] = 0;
    }
    message->type = packet[5];
    if (gh_decode_payload(message->type, packet + GHC_GAME_HEADER_SIZE, message) != 0) return -1;
    return gh_valid_message(message) ? 0 : -1;
}
