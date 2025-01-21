#ifndef VTFS_HTTP_H
#define VTFS_HTTP_H

#include <linux/inet.h>
#include "vtfs.h"

int64_t vtfs_http_call(const char *token, const char *method,
                            char *response_buffer, size_t buffer_size,
                            size_t arg_size, ...);

void encode(const char *, char *);

struct __attribute__((__packed__)) lookup_response
{
  uint32_t inode;
  uint32_t mode;
};

struct __attribute__((__packed__)) create_response
{
  uint32_t inode;
};

struct __attribute__((__packed__)) unlink_response
{

};

struct __attribute__((__packed__)) r_dentry {
  char name[MAX_DENTRY_NAME_LEN];
  uint32_t inode;
  uint32_t mode;
};

struct __attribute__((__packed__)) iterate_response
{
  uint32_t count;
  struct r_dentry dentries[MAX_DENTRY_NUM];
};

struct __attribute__((__packed__)) read_response
{
  uint32_t size;
  char data[MAX_FILE_SIZE];
};

struct __attribute__((__packed__)) write_response
{

};

#endif // VTFS_HTTP_H
