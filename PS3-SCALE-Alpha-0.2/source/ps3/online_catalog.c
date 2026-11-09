#include "ps3scale/online.h"

#include <curl/curl.h>
#include <net/net.h>
#include <net/netctl.h>
#include <sysmodule/sysmodule.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PS3SCALE_CA_BUNDLE_PATH \
    "/dev_hdd0/game/PS3SCALE1/USRDIR/cacert.pem"
#define PS3SCALE_DOWNLOAD_LIMIT (64L * 1024L)
#define PS3SCALE_DOWNLOAD_HARD_LIMIT (2L * 1024L * 1024L)
#define PS3SCALE_USER_AGENT "PS3SCALE/0.3"

typedef struct download_context {
    FILE *file;
    size_t bytes_written;
    size_t max_bytes;
} download_context_t;

static size_t write_download_data(void *contents, size_t size,
                                  size_t count, void *user_data)
{
    download_context_t *context = (download_context_t *)user_data;
    size_t bytes;

    if (size && count > ((size_t)-1) / size)
        return 0;
    bytes = size * count;
    if (!context || !context->file || !context->max_bytes ||
        context->bytes_written > context->max_bytes ||
        bytes > context->max_bytes - context->bytes_written)
        return 0;
    if (fwrite(contents, 1, bytes, context->file) != bytes)
        return 0;
    context->bytes_written += bytes;
    return bytes;
}

static int network_modules_ready(void)
{
    int result;

    result = sysModuleLoad(SYSMODULE_NET);
    if (result < 0)
        return -1;

    result = sysModuleLoad(SYSMODULE_NETCTL);
    if (result < 0) {
        sysModuleUnload(SYSMODULE_NET);
        return -1;
    }

    result = netInitialize();
    if (result < 0) {
        sysModuleUnload(SYSMODULE_NETCTL);
        sysModuleUnload(SYSMODULE_NET);
        return -1;
    }

    result = netCtlInit();
    if (result < 0) {
        netDeinitialize();
        sysModuleUnload(SYSMODULE_NETCTL);
        sysModuleUnload(SYSMODULE_NET);
        return -1;
    }

    return 0;
}

static void network_modules_shutdown(void)
{
    netCtlTerm();
    netDeinitialize();
    sysModuleUnload(SYSMODULE_NETCTL);
    sysModuleUnload(SYSMODULE_NET);
}

int ps3scale_online_download_limited(
    const char *url,
    const char *destination,
    size_t max_bytes)
{
    CURL *curl = NULL;
    CURLcode curl_result;
    long response_code = 0;
    FILE *file = NULL;
    download_context_t download;
    char temporary_path[512];
    int result = -1;

    if (!url || strncmp(url, "https://", 8) != 0 ||
        !destination || !destination[0] ||
        !max_bytes || max_bytes > (size_t)PS3SCALE_DOWNLOAD_HARD_LIMIT ||
        strlen(destination) + 6 >= sizeof(temporary_path))
        return -1;

    if (network_modules_ready() != 0)
        return -1;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        network_modules_shutdown();
        return -1;
    }

    snprintf(temporary_path, sizeof(temporary_path), "%s.part", destination);
    unlink(temporary_path);
    file = fopen(temporary_path, "wb");
    if (!file)
        goto cleanup;
    memset(&download, 0, sizeof(download));
    download.file = file;
    download.max_bytes = max_bytes;

    curl = curl_easy_init();
    if (!curl)
        goto cleanup;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PS3SCALE_USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_download_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &download);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE, (long)max_bytes);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 4L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, PS3SCALE_CA_BUNDLE_PATH);
#ifdef CURLPROTO_HTTPS
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif

    curl_result = curl_easy_perform(curl);
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (curl_result == CURLE_OK && response_code == 200 &&
        download.bytes_written > 0 &&
        download.bytes_written <= max_bytes)
        result = 0;

cleanup:
    if (file) {
        if (fclose(file) != 0)
            result = -1;
        file = NULL;
    }
    if (curl)
        curl_easy_cleanup(curl);
    curl_global_cleanup();
    network_modules_shutdown();

    if (result == 0) {
        if (rename(temporary_path, destination) != 0)
            result = -1;
    }
    if (result != 0)
        unlink(temporary_path);

    return result;
}

int ps3scale_online_download(const char *url, const char *destination)
{
    return ps3scale_online_download_limited(
        url, destination, (size_t)PS3SCALE_DOWNLOAD_LIMIT);
}
