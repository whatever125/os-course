#include <linux/init.h>
#include <linux/module.h>
#include <linux/printk.h>

#define MODULE_NAME "hello"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("vityaman");
MODULE_DESCRIPTION("A simple Hello World kernel module");

#define log(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

static int __init hello_init(void) {
  log("Hello, world!\n");
  return 0;
}

static void __exit hello_exit(void) { log("Goodbye, world!\n"); }

module_init(hello_init);
module_exit(hello_exit);
