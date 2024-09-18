#ifndef NETWORK_FILE_SYSTEM_HTTP_H
#define NETWORK_FILE_SYSTEM_HTTP_H

#include <linux/inet.h>

int64_t networkfs_http_call(const char *token, const char *method,
                            char *response_buffer, size_t buffer_size,
                            size_t arg_size, ...);

void encode(const char *, char *);

#endif // NETWORK_FILE_SYSTEM_HTTP_H
