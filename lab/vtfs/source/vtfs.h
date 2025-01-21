#ifndef VTFS_H
#define VTFS_H

#define MODULE_NAME "vtfs"

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

#define MAX_FILE_SIZE         (1024)
#define MAX_DENTRY_NAME_LEN   (128)
#define MAX_DENTRY_NUM        (4)
#define ROOT_INO              (1000)

#define INT_LEN               (11)
#define MAX_DENTRY_NAME_BYTE  ((MAX_DENTRY_NAME_LEN - 1) * 3 + 1)
#define MAX_FILE_SIZE_BYTE    ((MAX_FILE_SIZE - 1) * 3 + 1)

#endif // VTFS_H
