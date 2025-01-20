#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/slab.h>

#define MODULE_NAME "vtfs"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("secs-dev");
MODULE_DESCRIPTION("A simple FS kernel module");

#define LOG(fmt, ...) pr_info("[" MODULE_NAME "]: " fmt, ##__VA_ARGS__)

#define MAX_FILE_SIZE         1024
#define MAX_DENTRY_NAME_LEN   128
#define ROOT_INO              1000

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
  struct vtfs_dentry *dentry;
  struct inode *inode;
  ino_t root;
  const char *name;

  root = parent_inode->i_ino;
  name = child_dentry->d_name.name;

  list_for_each_entry(dentry, &vtfs_sb.s_dentries, d_list) {
    if (dentry->d_parent_ino == root && strcmp(dentry->d_name, name) == 0) {
      inode = vtfs_get_inode(vtfs_sb.s_sb, parent_inode, dentry->d_inode->i_mode, dentry->d_inode->i_ino);
      if (!inode) {
        return ERR_PTR(-ENOMEM);
      }
      d_add(child_dentry, inode);
      return NULL;
    }
  }
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
  struct vtfs_inode *vtfs_inode;
  
  inode = vtfs_get_inode(parent_inode->i_sb, parent_inode, mode, next_ino++);
  if (!inode) {
    return -ENOMEM;
  }

  vtfs_dentry = kmalloc(sizeof(struct vtfs_dentry), GFP_KERNEL);
  if (!vtfs_dentry) {
    iput(inode);
    return -ENOMEM;
  }

  vtfs_inode = kmalloc(sizeof(struct vtfs_inode), GFP_KERNEL);
  if (!vtfs_inode) {
    kfree(vtfs_dentry);
    iput(inode);
    return -ENOMEM;
  }

  vtfs_dentry->d_dentry = child_dentry;
  strcpy(vtfs_dentry->d_name, child_dentry->d_name.name);
  vtfs_dentry->d_parent_ino = parent_inode->i_ino;
  vtfs_dentry->d_inode = vtfs_inode;
  vtfs_dentry->d_inode->i_ino = inode->i_ino;
  vtfs_dentry->d_inode->i_mode = inode->i_mode;
  vtfs_dentry->d_inode->i_size = 0;

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
  const char *name;

  root = parent_inode->i_ino;
  name = child_dentry->d_name.name;

  list_for_each_entry(dentry, &vtfs_sb.s_dentries, d_list) {
    if (dentry->d_parent_ino == root && strcmp(dentry->d_name, name) == 0) {
      drop_nlink(child_dentry->d_inode);
      if (child_dentry->d_inode->i_nlink == 0) {
        kfree(dentry->d_inode);
      }
      list_del(&dentry->d_list);
      kfree(dentry);
      d_drop(child_dentry);
      return 0;
    }
  }
  return -ENOENT;
}

int vtfs_mkdir(
  struct inode *parent_inode,
  struct dentry *child_dentry,
  umode_t mode
) {
  struct inode *inode;
  struct vtfs_dentry *vtfs_dentry;
  struct vtfs_inode *vtfs_inode;
  
  inode = vtfs_get_inode(parent_inode->i_sb, parent_inode, mode | S_IFDIR, next_ino++);
  if (!inode) {
    return -ENOMEM;
  }

  vtfs_dentry = kmalloc(sizeof(struct vtfs_dentry), GFP_KERNEL);
  if (!vtfs_dentry) {
    iput(inode);
    return -ENOMEM;
  }

  vtfs_inode = kmalloc(sizeof(struct vtfs_inode), GFP_KERNEL);
  if (!vtfs_inode) {
    kfree(vtfs_dentry);
    iput(inode);
    return -ENOMEM;
  }

  vtfs_inode->i_ino = inode->i_ino;
  vtfs_inode->i_mode = inode->i_mode;
  vtfs_inode->i_size = 0;

  vtfs_dentry->d_dentry = child_dentry;
  strcpy(vtfs_dentry->d_name, child_dentry->d_name.name);
  vtfs_dentry->d_parent_ino = parent_inode->i_ino;
  vtfs_dentry->d_inode = vtfs_inode;

  if (vtfs_dentry->d_parent_ino == vtfs_dentry->d_inode->i_ino) {
    kfree(vtfs_inode);
    kfree(vtfs_dentry);
    iput(inode);
    return -EFAULT;
  }

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
  const char *name;

  list_for_each_entry(dentry, &vtfs_sb.s_dentries, d_list) {
    if (dentry->d_parent_ino == child_dentry->d_inode->i_ino) {
      return -ENOTEMPTY;
    }
  }

  root = parent_inode->i_ino;
  name = child_dentry->d_name.name;

  list_for_each_entry(dentry, &vtfs_sb.s_dentries, d_list) {
    if (dentry->d_parent_ino == root && strcmp(dentry->d_name, name) == 0) {
      list_del(&dentry->d_list);
      kfree(dentry->d_inode);
      kfree(dentry);
      drop_nlink(child_dentry->d_inode);
      drop_nlink(parent_inode);
      d_drop(child_dentry);
      return 0;
    }
  }

  return -ENOENT;
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
  struct vtfs_dentry *dentry;
  struct inode *cur_dir_inode;
  unsigned type;

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

  list_for_each_entry(dentry, &vtfs_sb.s_dentries, d_list) {
    if (dentry->d_parent_ino == cur_dir_inode->i_ino) {
      if (S_ISDIR(dentry->d_inode->i_mode)) {
        type = DT_DIR;
      } else if (S_ISREG(dentry->d_inode->i_mode)) {
        type = DT_REG;
      } else {
        type = DT_UNKNOWN;
      }
      if (!dir_emit(ctx, dentry->d_name, strlen(dentry->d_name), dentry->d_inode->i_ino, type)) {
        return -ENOMEM;
      }
      ctx->pos++;
    }
  }
  return ctx->pos;
}

ssize_t vtfs_read(
  struct file *file,
  char *buffer,
  size_t len,
  loff_t *offset
) {
  struct vtfs_dentry *dentry;
  struct inode *file_inode;
  size_t max_len;

  if (!file || !buffer || !offset || *offset < 0) {
    return -EINVAL;
  }

  file_inode = file->f_path.dentry->d_inode;

  list_for_each_entry(dentry, &vtfs_sb.s_dentries, d_list) {
    if (dentry->d_inode->i_ino == file_inode->i_ino) {
      if (*offset >= dentry->d_inode->i_size){
        return 0;
      }
      max_len = dentry->d_inode->i_size - *offset;
      if (len > max_len) {
        len = max_len;
      }
      if (copy_to_user(buffer, dentry->d_inode->i_data + *offset, len) != 0) {
        return -EFAULT;
      }
      *offset += len;
      return len;
    }
  }
  return -ENOENT;
}

ssize_t vtfs_write(
  struct file *file,
  const char *buffer,
  size_t len,
  loff_t *offset
) {
  struct vtfs_dentry *dentry;
  struct inode *file_inode;
  size_t max_len;

  if (!file || !buffer || !offset || *offset < 0) {
    return -EINVAL;
  }

  file_inode = file->f_path.dentry->d_inode;

  list_for_each_entry(dentry, &vtfs_sb.s_dentries, d_list) {
    if (dentry->d_inode->i_ino == file_inode->i_ino) {
      if (*offset >= MAX_FILE_SIZE) {
        return -ENOSPC;
      }
      if (*offset == 0) {
        dentry->d_inode->i_size = 0;
        memset(dentry->d_inode->i_data, 0, MAX_FILE_SIZE);
      }
      max_len = MAX_FILE_SIZE - *offset;
      if (len > max_len) {
        len = max_len;
      }
      if (copy_from_user(dentry->d_inode->i_data + *offset, buffer, len) != 0) {
        return -EFAULT;
      }
      *offset += len;
      if (*offset > dentry->d_inode->i_size) {
        dentry->d_inode->i_size = *offset;
      }
      return len;
    }
  }
  return -ENOENT;
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
