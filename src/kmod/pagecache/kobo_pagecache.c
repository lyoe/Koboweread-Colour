// SPDX-License-Identifier: GPL-2.0
/*
 * kobo_pagecache: drop the clean page cache of one regular file.
 *
 * The Kobo kernel is built without CONFIG_ADVISE_SYSCALLS, so
 * posix_fadvise(POSIX_FADV_DONTNEED) is unavailable. The Android image is an
 * ext4 filesystem on a loop device backed by a file on vfat, so every page
 * read through it is cached twice: in ext4 and in the backing file. Dropping
 * the backing file's copy periodically leaves that memory to the ext4 cache.
 *
 * ioctl(KOBO_PAGECACHE_DROP, fd) calls invalidate_mapping_pages() on the
 * file, which skips dirty, writeback, mapped and locked pages; no data can be
 * lost. Only root (CAP_SYS_ADMIN) may use the device.
 */
#include <linux/capability.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>

#define KOBO_PAGECACHE_DROP _IOW('k', 0x50, int)

static long kobo_pagecache_ioctl(struct file *self, unsigned int cmd, unsigned long arg)
{
	struct file *target;
	unsigned long dropped;

	if (cmd != KOBO_PAGECACHE_DROP)
		return -ENOTTY;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	target = fget((unsigned int)arg);
	if (!target)
		return -EBADF;
	if (!S_ISREG(file_inode(target)->i_mode)) {
		fput(target);
		return -EINVAL;
	}
	dropped = invalidate_mapping_pages(target->f_mapping, 0, -1);
	fput(target);
	return dropped > LONG_MAX ? LONG_MAX : (long)dropped;
}

static const struct file_operations kobo_pagecache_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = kobo_pagecache_ioctl,
};

static struct miscdevice kobo_pagecache_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "kobo_pagecache",
	.fops = &kobo_pagecache_fops,
	.mode = 0600,
};

static int __init kobo_pagecache_init(void)
{
	return misc_register(&kobo_pagecache_dev);
}

static void __exit kobo_pagecache_exit(void)
{
	misc_deregister(&kobo_pagecache_dev);
}

module_init(kobo_pagecache_init);
module_exit(kobo_pagecache_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Drop clean page cache of a file (no fadvise on this kernel)");
