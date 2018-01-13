#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/fs_struct.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/kernel.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("");
MODULE_DESCRIPTION("");
MODULE_VERSION("0.1");


void list_recursively(struct dentry* parent) {
	//S_ISDIR(d.d_inode.i_mode)
	struct dentry* child;
	struct dentry* tmp;
	char buf[256];
	list_for_each_entry_safe(child, tmp, &parent->d_subdirs, d_child) {
		if (d_is_negative(child)) {
			continue;
		}

		const char* path = dentry_path_raw(child, buf, sizeof(buf));
		printk(KERN_INFO "FSS: %s\n", path);
		if (S_ISDIR(child->d_inode->i_mode)) {
			list_recursively(child);
		}
	}
}

int init_module(void) {
	list_recursively(current->fs->root.mnt->mnt_root);

	//struct file_system_type *ext4_fs = get_fs_type("ext4");
	//struct vfsmount* mount = vfs_kern_mount(ext4_fs, MNT_READONLY, "/root/ext4.img", NULL);
	//if (!IS_ERR_OR_NULL(mount)) {
	//	list_recursively(mount->mnt_root);
	//	kern_unmount(mount);
	//} else {
	//	printk(KERN_INFO "FSS: mount failed: %ld\n", -(long)mount);
	//}

	return 0;
}
void cleanup_module(void) {
	printk(KERN_INFO "Bye world.\n");
}
