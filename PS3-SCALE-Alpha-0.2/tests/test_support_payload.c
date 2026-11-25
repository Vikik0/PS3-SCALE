#include "ps3scale/support_payload.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define VALID_ID "01234567-89ab-4cde-8fab-0123456789ab"

int main(void)
{
    char json[8192];
    char title[PS3SCALE_SUPPORT_TITLE_MAX + 2];
    char details[PS3SCALE_SUPPORT_DETAILS_MAX + 2];
    const char *expected =
        "{\"request_id\":\"" VALID_ID "\","
        "\"kind\":\"issue\","
        "\"title\":\"Crash \\\"A\\\"\","
        "\"details\":\"line 1\\nline 2 \\\\ path\\tمرحبا\","
        "\"model\":\"CECH-25xx\","
        "\"firmware\":\"4.91 HEN\","
        "\"platform\":\"CEX\","
        "\"app_version\":\"ALPHA 0.32\"}";

    assert(ps3scale_support_build_json(
        json, sizeof(json), VALID_ID, "issue", "Crash \"A\"",
        "line 1\nline 2 \\ path\tمرحبا", "CECH-25xx",
        "4.91 HEN", "CEX", "ALPHA 0.32") == 0);
    assert(strcmp(json, expected) == 0);

    memset(title, 'T', sizeof(title) - 2);
    title[sizeof(title) - 2] = '\0';
    memset(details, 'D', sizeof(details) - 2);
    details[sizeof(details) - 2] = '\0';
    assert(ps3scale_support_build_json(
        json, sizeof(json), VALID_ID, "feedback", title, details,
        "UNKNOWN", "UNKNOWN", "UNKNOWN", "ALPHA 0.32") == 0);
    assert(strlen(json) < sizeof(json));
    assert(strstr(json, "\"kind\":\"feedback\"") != NULL);
    assert(strstr(json, details) != NULL);

    title[PS3SCALE_SUPPORT_TITLE_MAX] = 'T';
    title[PS3SCALE_SUPPORT_TITLE_MAX + 1] = '\0';
    assert(ps3scale_support_build_json(
        json, sizeof(json), VALID_ID, "issue", title, "details",
        "model", "firmware", "platform", "version") == -1);

    memset(details, 'D', sizeof(details) - 1);
    details[sizeof(details) - 1] = '\0';
    details[PS3SCALE_SUPPORT_DETAILS_MAX] = 'D';
    details[PS3SCALE_SUPPORT_DETAILS_MAX + 1] = '\0';
    assert(ps3scale_support_build_json(
        json, sizeof(json), VALID_ID, "issue", "title", details,
        "model", "firmware", "platform", "version") == -1);

    assert(ps3scale_support_build_json(
        json, sizeof(json), "not-a-uuid", "issue", "title", "details",
        "model", "firmware", "platform", "version") == -1);
    assert(ps3scale_support_build_json(
        json, sizeof(json), VALID_ID, "invalid", "title", "details",
        "model", "firmware", "platform", "version") == -1);
    assert(ps3scale_support_build_json(
        json, 8, VALID_ID, "issue", "title", "details",
        "model", "firmware", "platform", "version") == -1);
    assert(ps3scale_support_build_json(
        NULL, 0, VALID_ID, "issue", "title", "details",
        "model", "firmware", "platform", "version") == -1);

    puts("PS3 SCALE support payload tests: PASS");
    return 0;
}
