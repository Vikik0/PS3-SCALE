#ifndef PS3SCALE_ONLINE_H
#define PS3SCALE_ONLINE_H

/* Downloads a fixed HTTPS resource to a local file using verified TLS. */
int ps3scale_online_download(const char *url, const char *destination);

#endif
