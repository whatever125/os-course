#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#define MODULE_NAME "vtfs"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("vtshniks");
MODULE_DESCRIPTION("A simple FS kernel module");

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

static int __init vtfs_init(void) {
  LOG("VT Flood!\n");
  return 0;
}

static void __exit vtfs_exit(void) {
  LOG("VT Mute!\n");
}

module_init(vtfs_init);  // NOLINT
module_exit(vtfs_exit);  // NOLINT
