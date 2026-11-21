#include "tls_client.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#if defined(__powerpc64__)
#include <lv2/system.h>
#endif

extern const unsigned char cb_ca_bundle_pem[];
extern const size_t cb_ca_bundle_pem_len;

struct CbTlsClient {
    int fd;
    int ssl_initialized;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config config;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context drbg;
};

static mbedtls_x509_crt cb_ca_chain;
static int cb_ca_state;

static uint64_t cb_now_ms(void)
{
    struct timeval now;
    if (gettimeofday(&now, NULL) != 0) return 0;
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_usec / 1000u;
}

static int cb_remaining_ms(uint64_t deadline)
{
    uint64_t now = cb_now_ms();
    uint64_t remaining;
    if (now >= deadline) return 0;
    remaining = deadline - now;
    return remaining > (uint64_t)INT_MAX ? INT_MAX : (int)remaining;
}

static int cb_wait_socket(int fd, int write_ready, int timeout_ms)
{
    fd_set read_set;
    fd_set write_set;
    struct timeval timeout;
    int result;

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    if (write_ready) FD_SET(fd, &write_set);
    else FD_SET(fd, &read_set);
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    do {
        result = select(fd + 1, write_ready ? NULL : &read_set,
                        write_ready ? &write_set : NULL, NULL, &timeout);
    } while (result < 0 && errno == EINTR);

    if (result > 0) return 1;
    return result == 0 ? 0 : -1;
}

static int cb_entropy_source(void *context, unsigned char *output,
                             size_t length, size_t *output_length)
{
    size_t offset = 0;
    (void)context;

#if defined(__powerpc64__)
    while (offset < length) {
        size_t chunk = length - offset;
        if (chunk > 256u) chunk = 256u;
        if (sysGetRandomNumber(output + offset, (u64)chunk) != 0) {
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }
        offset += chunk;
    }
#else
    {
        int random_fd = open("/dev/urandom", O_RDONLY);
        if (random_fd < 0) return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        while (offset < length) {
            ssize_t count = read(random_fd, output + offset, length - offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) {
                close(random_fd);
                return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
            }
            offset += (size_t)count;
        }
        close(random_fd);
    }
#endif

    *output_length = length;
    return 0;
}

static int cb_tls_send(void *context, const unsigned char *buffer, size_t length)
{
    int fd = *(const int *)context;
    ssize_t count = send(fd, buffer, length, 0);
    if (count >= 0) return (int)count;
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int cb_tls_receive(void *context, unsigned char *buffer, size_t length)
{
    int fd = *(const int *)context;
    ssize_t count = recv(fd, buffer, length, 0);
    if (count >= 0) return (int)count;
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

static int cb_ca_init(void)
{
    int result;
    if (cb_ca_state == 1) return 0;
    if (cb_ca_state < 0) return -1;

    mbedtls_x509_crt_init(&cb_ca_chain);
    result = mbedtls_x509_crt_parse(&cb_ca_chain, cb_ca_bundle_pem,
                                   cb_ca_bundle_pem_len);
    if (result < 0 || cb_ca_chain.raw.p == NULL) {
        mbedtls_x509_crt_free(&cb_ca_chain);
        cb_ca_state = -1;
        return -1;
    }

    /* Some roots may be skipped; successfully parsed certificates remain usable. */
    cb_ca_state = 1;
    return 0;
}

static int cb_open_tcp(const char *host, unsigned short port, int timeout_ms)
{
    struct addrinfo hints;
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    char service[8];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (snprintf(service, sizeof(service), "%u", (unsigned int)port) < 0) return -1;
    if (getaddrinfo(host, service, &hints, &addresses) != 0) return -1;

    for (address = addresses; address; address = address->ai_next) {
        int flags;
        int connect_result;
        int socket_error = 0;
        socklen_t socket_error_length = sizeof(socket_error);
        struct timeval timeout;

        fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0) continue;

        flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            close(fd);
            fd = -1;
            continue;
        }

        connect_result = connect(fd, address->ai_addr, (socklen_t)address->ai_addrlen);
        if (connect_result != 0 && errno == EINPROGRESS) {
            int ready = cb_wait_socket(fd, 1, timeout_ms);
            if (ready > 0 &&
                getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error,
                           &socket_error_length) == 0 &&
                socket_error == 0) {
                connect_result = 0;
            } else {
                connect_result = -1;
            }
        }

        if (connect_result != 0 || fcntl(fd, F_SETFL, flags) < 0) {
            close(fd);
            fd = -1;
            continue;
        }

        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        break;
    }

    freeaddrinfo(addresses);
    return fd;
}

int cb_tls_connect(CbTlsClient **out, const char *host, unsigned short port,
                   int timeout_ms)
{
    CbTlsClient *client;
    uint64_t deadline;
    int result;
    int fd;

    if (!out || !host || !*host || port == 0) return -1;
    *out = NULL;
    if (timeout_ms <= 0) timeout_ms = 5000;
    if (cb_ca_init() != 0) return -1;

    client = (CbTlsClient *)calloc(1, sizeof(*client));
    if (!client) return -1;
    client->fd = -1;

    mbedtls_ssl_init(&client->ssl);
    mbedtls_ssl_config_init(&client->config);
    mbedtls_entropy_init(&client->entropy);
    mbedtls_ctr_drbg_init(&client->drbg);

    result = mbedtls_entropy_add_source(&client->entropy, cb_entropy_source,
                                        NULL, 32u, MBEDTLS_ENTROPY_SOURCE_STRONG);
    if (result != 0) goto fail;

    result = mbedtls_ctr_drbg_seed(&client->drbg, mbedtls_entropy_func,
                                   &client->entropy,
                                   (const unsigned char *)"CrashBashEnhanced", 17u);
    if (result != 0) goto fail;

    result = mbedtls_ssl_config_defaults(&client->config, MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT);
    if (result != 0) goto fail;

    mbedtls_ssl_conf_authmode(&client->config, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&client->config, &cb_ca_chain, NULL);
    mbedtls_ssl_conf_rng(&client->config, mbedtls_ctr_drbg_random, &client->drbg);
    mbedtls_ssl_conf_min_version(&client->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&client->config, MBEDTLS_SSL_MAJOR_VERSION_3,
                                 MBEDTLS_SSL_MINOR_VERSION_3);

    result = mbedtls_ssl_setup(&client->ssl, &client->config);
    if (result != 0) goto fail;
    client->ssl_initialized = 1;

    result = mbedtls_ssl_set_hostname(&client->ssl, host);
    if (result != 0) goto fail;

    fd = cb_open_tcp(host, port, timeout_ms);
    if (fd < 0) goto fail;
    client->fd = fd;
    mbedtls_ssl_set_bio(&client->ssl, &client->fd,
                        cb_tls_send, cb_tls_receive, NULL);

    deadline = cb_now_ms() + (uint64_t)timeout_ms;
    for (;;) {
        result = mbedtls_ssl_handshake(&client->ssl);
        if (result == 0) break;
        if (result != MBEDTLS_ERR_SSL_WANT_READ &&
            result != MBEDTLS_ERR_SSL_WANT_WRITE) goto fail;

        {
            int remaining = cb_remaining_ms(deadline);
            int ready;
            if (remaining <= 0) goto fail;
            ready = cb_wait_socket(client->fd,
                                   result == MBEDTLS_ERR_SSL_WANT_WRITE,
                                   remaining);
            if (ready <= 0) goto fail;
        }
    }

    if (mbedtls_ssl_get_verify_result(&client->ssl) != 0) goto fail;
    *out = client;
    return 0;

fail:
    cb_tls_close(client);
    return -1;
}

int cb_tls_send_all(CbTlsClient *client, const void *data, size_t length,
                    int timeout_ms)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t sent = 0;
    uint64_t deadline;

    if (!client || client->fd < 0 || (!data && length != 0)) return -1;
    if (timeout_ms <= 0) timeout_ms = 5000;
    deadline = cb_now_ms() + (uint64_t)timeout_ms;

    while (sent < length) {
        int result = mbedtls_ssl_write(&client->ssl, bytes + sent, length - sent);
        if (result > 0) {
            sent += (size_t)result;
            continue;
        }
        if (result != MBEDTLS_ERR_SSL_WANT_READ &&
            result != MBEDTLS_ERR_SSL_WANT_WRITE) return -1;

        {
            int remaining = cb_remaining_ms(deadline);
            int ready;
            if (remaining <= 0) return -1;
            ready = cb_wait_socket(client->fd,
                                   result == MBEDTLS_ERR_SSL_WANT_WRITE,
                                   remaining);
            if (ready <= 0) return -1;
        }
    }
    return 0;
}

ssize_t cb_tls_recv(CbTlsClient *client, void *buffer, size_t capacity,
                    int timeout_ms)
{
    uint64_t deadline;

    if (!client || client->fd < 0 || !buffer || capacity == 0) return -1;
    if (timeout_ms <= 0) timeout_ms = 5000;
    deadline = cb_now_ms() + (uint64_t)timeout_ms;

    for (;;) {
        int result;
        int remaining;
        int ready;

        if (mbedtls_ssl_get_bytes_avail(&client->ssl) == 0) {
            remaining = cb_remaining_ms(deadline);
            if (remaining <= 0) return 0;
            ready = cb_wait_socket(client->fd, 0, remaining);
            if (ready == 0) return 0;
            if (ready < 0) return -1;
        }

        result = mbedtls_ssl_read(&client->ssl, (unsigned char *)buffer, capacity);
        if (result > 0) return (ssize_t)result;
        if (result == 0 || result == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY ||
            result == MBEDTLS_ERR_SSL_CONN_EOF) return -1;
        if (result != MBEDTLS_ERR_SSL_WANT_READ &&
            result != MBEDTLS_ERR_SSL_WANT_WRITE) return -1;

        remaining = cb_remaining_ms(deadline);
        if (remaining <= 0) return 0;
        ready = cb_wait_socket(client->fd,
                               result == MBEDTLS_ERR_SSL_WANT_WRITE,
                               remaining);
        if (ready == 0) return 0;
        if (ready < 0) return -1;
    }
}

int cb_tls_fd(const CbTlsClient *client)
{
    return client ? client->fd : -1;
}

void cb_tls_close(CbTlsClient *client)
{
    if (!client) return;
    if (client->ssl_initialized) (void)mbedtls_ssl_close_notify(&client->ssl);
    if (client->fd >= 0) {
        (void)shutdown(client->fd, SHUT_RDWR);
        close(client->fd);
    }
    mbedtls_ssl_free(&client->ssl);
    mbedtls_ssl_config_free(&client->config);
    mbedtls_ctr_drbg_free(&client->drbg);
    mbedtls_entropy_free(&client->entropy);
    free(client);
}

void cb_tls_global_cleanup(void)
{
    if (cb_ca_state == 1) mbedtls_x509_crt_free(&cb_ca_chain);
    cb_ca_state = 0;
}
