#ifndef CB_TLS_CLIENT_H
#define CB_TLS_CLIENT_H
#include <stddef.h>
#include <sys/types.h>
typedef struct CbTlsClient CbTlsClient;
int cb_tls_connect(CbTlsClient **out, const char *host, unsigned short port, int timeout_ms);
int cb_tls_send_all(CbTlsClient *client, const void *data, size_t length, int timeout_ms);
ssize_t cb_tls_recv(CbTlsClient *client, void *buffer, size_t capacity, int timeout_ms);
int cb_tls_fd(const CbTlsClient *client);
void cb_tls_close(CbTlsClient *client);
void cb_tls_global_cleanup(void);
#endif
