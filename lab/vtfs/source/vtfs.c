#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include "vtfs.h"
#include "http.c"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("secs-dev");
MODULE_DESCRIPTION("A simple FS kernel module");

unsigned long next_ino = ROOT_INO;

// VTFS STRUCTURES

struct vtfs_inode {
  ino_t   i_ino;                        // indode number
  umode_t i_mode;                       // user rights, etc
  size_t  i_size;                       // file size
  char    i_data[MAX_FILE_SIZE];        // file data
};

struct vtfs_dentry {
  struct dentry      *d_dentry;         // dentry
  char   d_name[MAX_DENTRY_NAME_LEN];   // dentry name
  int    d_parent_ino;                  // parent inode number
  struct vtfs_inode  *d_inode;          // inode
  struct list_head   d_list;            // common fs list
};

static struct {
  struct super_block *s_sb;             // main super block
  struct list_head   s_dentries;        // list of dentries in fs
} vtfs_sb;

struct inode* vtfs_get_inode(struct super_block*, const struct inode*, umode_t, int);

// INODE OPERATIONS

struct dentry* vtfs_lookup(
  struct inode* parent_inode,
  struct dentry* child_dentry,
  unsigned int flag
) {
  struct inode *inode;
  ino_t root;
  char root_str[INT_LEN];
  const char *name;
  char name_enc[MAX_DENTRY_NAME_BYTE];
  int64_t code;
  struct lookup_response response;

  root = parent_inode->i_ino;
  name = child_dentry->d_name.name;

  encode(name, name_enc);
  snprintf(root_str, sizeof(root_str), "%u", (uint) root);

  code = vtfs_http_call(
    "TODO",
    "lookup",
    (void *)(&response),
    sizeof(response),
    2,
    "name",
    name_enc,
    "parent_inode",
    root_str
  );

  if (code != 0) {
    printk(KERN_ERR "Lookup HTTP-call error code %lld\n", code);
    return NULL;
  }
  printk(KERN_INFO "Mode: %d\n", response.mode);
  inode = vtfs_get_inode(vtfs_sb.s_sb, parent_inode, response.mode, response.inode);
  d_add(child_dentry, inode);

  return NULL;
}

int vtfs_create(
  struct inode *parent_inode,
  struct dentry *child_dentry,
  umode_t mode,
  bool b
) {
  struct inode *inode;
  struct vtfs_dentry *vtfs_dentry;
  ino_t root;
  char root_str[INT_LEN];
  const char *name;
  char name_enc[MAX_DENTRY_NAME_BYTE];
  char mode_str[INT_LEN];
  int64_t code;
  struct create_response response;

  name = child_dentry->d_name.name;
  root = parent_inode->i_ino;
  mode |= S_IFREG;
  mode |= S_IRWXUGO;

  encode(name, name_enc);
  snprintf(root_str, sizeof(root_str), "%u", (uint) root);
  snprintf(mode_str, sizeof(mode_str), "%u", (uint) mode);

  printk(KERN_INFO "Mode: %s\n", mode_str);

  code = vtfs_http_call(
    "TODO",
    "create",
    (void *)(&response),
    sizeof(response),
    3,
    "name",
    name_enc,
    "parent_inode",
    root_str,
    "mode",
    mode_str
  );
  if (code != 0) {
    printk(KERN_ERR "Create HTTP-call error code %lld\n", code);
    return -code;
  }
  
  inode = vtfs_get_inode(parent_inode->i_sb, parent_inode, mode, response.inode);
  if (!inode) {
    return -ENOMEM;
  }

  vtfs_dentry = kmalloc(sizeof(struct vtfs_dentry), GFP_KERNEL);
  if (!vtfs_dentry) {
    iput(inode);
    return -ENOMEM;
  }

  vtfs_dentry->d_dentry = child_dentry;
  strcpy(vtfs_dentry->d_name, child_dentry->d_name.name);
  vtfs_dentry->d_parent_ino = parent_inode->i_ino;

  list_add(&vtfs_dentry->d_list, &vtfs_sb.s_dentries);
  d_add(child_dentry, inode);

  return 0;
}

int vtfs_unlink(
  struct inode *parent_inode,
  struct dentry *child_dentry
) {
  struct vtfs_dentry *dentry;
  ino_t root;
  char root_str[INT_LEN];
  const char *name;
  char name_enc[MAX_DENTRY_NAME_BYTE];
  int64_t code;
  struct unlink_response response;

  root = parent_inode->i_ino;
  name = child_dentry->d_name.name;

  encode(name, name_enc);
  snprintf(root_str, sizeof(root_str), "%u", (uint) root);

  code = vtfs_http_call(
    "TODO",
    "remove",
    (void *)(&response),
    sizeof(response),
    2,
    "name",
    name_enc,
    "parent_inode",
    root_str
  );

  if (code != 0) {
    printk(KERN_ERR "Unlink HTTP-call error code %lld\n", code);
    return -code;
  }

  list_for_each_entry(dentry, &vtfs_sb.s_dentries, d_list) {
    if (dentry->d_parent_ino == root && strcmp(dentry->d_name, name) == 0) {
      drop_nlink(child_dentry->d_inode);
      list_del(&dentry->d_list);
      kfree(dentry);
      d_drop(child_dentry);
      break;
    }
  }
  return 0;
}

int vtfs_mkdir(
  struct inode *parent_inode,
  struct dentry *child_dentry,
  umode_t mode
) {
  struct inode *inode;
  struct vtfs_dentry *vtfs_dentry;
  ino_t root;
  char root_str[INT_LEN];
  const char *name;
  char name_enc[MAX_DENTRY_NAME_BYTE];
  char mode_str[INT_LEN];
  int64_t code;
  struct create_response response;

  name = child_dentry->d_name.name;
  root = parent_inode->i_ino;
  mode |= S_IFDIR;
  mode |= S_IRWXUGO;

  encode(name, name_enc);
  snprintf(root_str, sizeof(root_str), "%u", (uint) root);
  snprintf(mode_str, sizeof(mode_str), "%u", (uint) mode);

  code = vtfs_http_call(
    "TODO",
    "create",
    (void *)(&response),
    sizeof(response),
    3,
    "name",
    name_enc,
    "parent_inode",
    root_str,
    "mode",
    mode_str
  );
  if (code != 0) {
    printk(KERN_ERR "Mkdir HTTP-call error code %lld\n", code);
    return -code;
  }
  
  inode = vtfs_get_inode(parent_inode->i_sb, parent_inode, mode, response.inode);
  if (!inode) {
    return -ENOMEM;
  }

  if (parent_inode->i_ino == inode->i_ino) {
    iput(inode);
    return -EFAULT;
  }

  vtfs_dentry = kmalloc(sizeof(struct vtfs_dentry), GFP_KERNEL);
  if (!vtfs_dentry) {
    iput(inode);
    return -ENOMEM;
  }

  vtfs_dentry->d_dentry = child_dentry;
  strcpy(vtfs_dentry->d_name, child_dentry->d_name.name);
  vtfs_dentry->d_parent_ino = parent_inode->i_ino;

  list_add(&vtfs_dentry->d_list, &vtfs_sb.s_dentries);
  d_add(child_dentry, inode);
  inc_nlink(parent_inode);

  return 0;
}

int vtfs_rmdir(
  struct inode *parent_inode,
  struct dentry *child_dentry
) {
  struct vtfs_dentry *dentry;
  ino_t root;
  char root_str[INT_LEN];
  const char *name;
  char name_enc[MAX_DENTRY_NAME_BYTE];
  int64_t code;
  struct lookup_response response;

  root = parent_inode->i_ino;
  name = child_dentry->d_name.name;

  encode(name, name_enc);
  snprintf(root_str, sizeof(root_str), "%u", (uint) root);

  code = vtfs_http_call(
    "TODO",
    "remove",
    (void *)(&response),
    sizeof(response),
    2,
    "name",
    name_enc,
    "parent_inode",
    root_str
  );

  if (code != 0) {
    printk(KERN_ERR "Rmdir HTTP-call error code %lld\n", code);
    return -code;
  }

  list_for_each_entry(dentry, &vtfs_sb.s_dentries, d_list) {
    if (dentry->d_parent_ino == root && strcmp(dentry->d_name, name) == 0) {
      list_del(&dentry->d_list);
      kfree(dentry);
      drop_nlink(child_dentry->d_inode);
      drop_nlink(parent_inode);
      d_drop(child_dentry);
      break;
    }
  }
  return 0;
}

struct inode_operations vtfs_inode_ops = {
  .lookup = vtfs_lookup,
  .create = vtfs_create,
  .unlink = vtfs_unlink,
  .mkdir  = vtfs_mkdir,
  .rmdir  = vtfs_rmdir,
};

// FILE OPERATIONS

int vtfs_iterate(
  struct file* file,
  struct dir_context* ctx
) {
  struct inode *cur_dir_inode;
  unsigned type;
  char root_str[INT_LEN];
  int64_t code;
  struct iterate_response response;
  int i;

  if (file == NULL || ctx == NULL) {
    return -EINVAL;
  }
  if (!dir_emit_dots(file, ctx)) {
    return 0;
  }
  if (ctx->pos > 2) {
    return ctx->pos;
  }

  cur_dir_inode = file->f_path.dentry->d_inode;

  snprintf(root_str, sizeof(root_str), "%u", (uint) cur_dir_inode->i_ino);

  code = vtfs_http_call(
    "TODO",
    "iterate",
    (void *)(&response),
    sizeof(response),
    1,
    "parent_inode",
    root_str
  );
  if (code != 0) {
    printk(KERN_ERR "Iterate error code %lld\n", code);
    return -code;
  }

  for (i = 0; i < response.count; i++) {
    if (S_ISDIR(response.dentries[i].mode)) {
      type = DT_DIR;
    } else if (S_ISREG(response.dentries[i].mode)) {
      type = DT_REG;
    } else {
      type = DT_UNKNOWN;
    }
    printk(KERN_INFO "Mode: %d\n", response.dentries[i].mode);
    printk(KERN_INFO "Type: %d\n", type);
    if (!dir_emit(ctx, response.dentries[i].name, strlen(response.dentries[i].name), response.dentries[i].inode, response.dentries[i].mode | S_IRWXUGO)) {
      return -ENOMEM;
    }
    ctx->pos++;
  }

  return (int)(ctx->pos - file->f_pos);
}

ssize_t vtfs_read(
  struct file *file,
  char *buffer,
  size_t len,
  loff_t *offset
) {
  struct inode *file_inode;
  size_t max_len;
  char inode_str[INT_LEN];
  int64_t code;
  struct read_response response;

  if (!file || !buffer || !offset || *offset < 0) {
    return -EINVAL;
  }

  file_inode = file->f_path.dentry->d_inode;
  snprintf(inode_str, sizeof(inode_str), "%u", (uint) file_inode->i_ino);

  code = vtfs_http_call(
    "TODO",
    "read",
    (void *)(&response),
    sizeof(response),
    1,
    "inode",
    inode_str
  );
  if (code != 0) {
    printk(KERN_ERR "Read error code %lld\n", code);
    return -code;
  }
  pr_info("vtfs_read: buffer size = %zu, requested size = %zu\n", response.size, len);

  if (*offset >= response.size){
    return 0;
  }
  max_len = response.size - *offset;
  if (len > max_len) {
    len = max_len;
  }
  pr_info("vtfs_read: buffer size = %zu, requested size = %zu\n", response.size, len);
  if (copy_to_user(buffer, response.data + *offset, len) != 0) {
    return -EFAULT;
  }
  *offset += len;
  return len;
}

ssize_t vtfs_write(
  struct file *file,
  const char *buffer,
  size_t len,
  loff_t *offset
) {
  struct inode *file_inode;
  size_t max_len;
  char inode_str[INT_LEN];
  char data[MAX_FILE_SIZE];
  char data_enc[MAX_DENTRY_NAME_BYTE];
  char offset_str[INT_LEN];
  int64_t code;
  struct write_response response;

  if (!file || !buffer || !offset || *offset < 0) {
    return -EINVAL;
  }
  if (*offset >= MAX_FILE_SIZE) {
    return -ENOSPC;
  }

  pr_info("vtfs_write: offset = %zu, len = %zu\n", *offset, len);

  file_inode = file->f_path.dentry->d_inode;
  snprintf(inode_str, sizeof(inode_str), "%u", (uint) file_inode->i_ino);

  max_len = MAX_FILE_SIZE - *offset;
  if (len > max_len) {
    len = max_len;
  }
  if (copy_from_user(data, buffer, len) != 0) {
    return -EFAULT;
  }
  data[len] = 0;
  encode(data, data_enc);

  snprintf(offset_str, sizeof(offset_str), "%d", (int) *offset);

  pr_info("vtfs_write: offset = %zu, len = %zu\n", *offset, len);

  code = vtfs_http_call(
    "TODO",
    "write",
    (void *)(&response),
    sizeof(response),
    3,
    "inode",
    inode_str,
    "data",
    data_enc,
    "offset",
    offset_str
  );
  if (code != 0) {
    printk(KERN_ERR "Write error code %lld\n", code);
    return -code;
  }

  *offset += len;
  return len;
}

struct file_operations vtfs_dir_ops = {
  .iterate = vtfs_iterate,
  .read    = vtfs_read,
  .write   = vtfs_write,
};

// VFS OPERATIONS

struct inode* vtfs_get_inode(
  struct super_block* sb,
  const struct inode* dir,
  umode_t mode,
  int i_ino
) {
  struct inode *inode;
  if ((inode = new_inode(sb)) == NULL) {
    return NULL;
  }
  inode_init_owner(inode, dir, mode);
  inode->i_ino = i_ino;
  inode->i_op = &vtfs_inode_ops;
  inode->i_fop = &vtfs_dir_ops;
  if (S_ISDIR(mode)) {
    inc_nlink(inode);
  }
  return inode;
}

int vtfs_fill_super(
  struct super_block *sb,
  void *data,
  int silent
) {
  struct inode* inode = vtfs_get_inode(sb, NULL, S_IFDIR | S_IRWXUGO, next_ino++);
  if (!inode) {
    printk(KERN_ERR "Can't create root");
    return -ENOMEM;
  }
  sb->s_root = d_make_root(inode);
  if (sb->s_root == NULL) {
    printk(KERN_ERR "Can't create root");
    return -ENOMEM;
  }
  vtfs_sb.s_sb = sb;
  INIT_LIST_HEAD(&vtfs_sb.s_dentries);
  printk(KERN_INFO "Successfully created superblock\n");
  return 0;
}

struct dentry* vtfs_mount(
  struct file_system_type* fs_type,
  int flags,
  const char* token,
  void* data
) {
  struct dentry* ret = mount_nodev(fs_type, flags, data, vtfs_fill_super);
  if (ret == NULL) {
    printk(KERN_ERR "Can't mount file system");
  } else {
    printk(KERN_INFO "Mounted successfuly");
  }
  return ret;
}

// VTFS INIT

void vtfs_kill_sb(struct super_block* sb) {
  printk(KERN_INFO "vtfs super block is destroyed. Unmount successfully.\n");
}

static struct file_system_type vtfs_fs_type = {
  .name = "vtfs",
  .mount = vtfs_mount,
  .kill_sb = vtfs_kill_sb,
};

static int __init vtfs_init(void) {
  register_filesystem(&vtfs_fs_type);
  LOG("VTFS joined the kernel\n");
  return 0;
}

static void __exit vtfs_exit(void) {
  struct vtfs_dentry *dentry;
  struct vtfs_dentry *tmp;

  list_for_each_entry_safe(dentry, tmp, &vtfs_sb.s_dentries, d_list) {
    kfree(dentry->d_inode);
    kfree(dentry);
  }

  unregister_filesystem(&vtfs_fs_type);
  LOG("VTFS left the kernel\n");
}

module_init(vtfs_init);
module_exit(vtfs_exit);
