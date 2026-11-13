// SPDX-License-Identifier: GPL-2.0
// MADV_DONTNEED semantics follow Linux 4.1 mm/madvise.c.
// This is an opt-in ioctl for the caller's own address space, not a syscall hook.
#include <linux/module.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/kallsyms.h>
#include <linux/ioctl.h>
struct kobo_discard { __u32 start; __u32 length; };
#define KOBO_DISCARD _IOW('K', 0x41, struct kobo_discard)
struct kobo_hint { __u32 start, length, advice; };
#define KOBO_HINT _IOW('K', 0x42, struct kobo_hint)
static int (*split_range)(struct mm_struct *, struct vm_area_struct *, unsigned long, int);
static void (*zap_range)(struct vm_area_struct *, unsigned long, unsigned long, struct zap_details *);
static long discard_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
 struct kobo_hint req = {0};
 struct mm_struct *mm = current->mm;
 struct vm_area_struct *vma;
 unsigned long start, length, end, stop;
 int error = 0, hole = 0, write_lock;
 unsigned long flags;
 if (cmd != KOBO_DISCARD && cmd != KOBO_HINT) return -ENOTTY;
 if (copy_from_user(&req, (void __user *)arg, cmd == KOBO_DISCARD ? sizeof(struct kobo_discard) : sizeof(req))) return -EFAULT;
 if (cmd == KOBO_DISCARD) req.advice = MADV_DONTNEED;
 else if (req.advice > MADV_SEQUENTIAL) return -EINVAL;
 write_lock = req.advice != MADV_DONTNEED;
 if (!mm) return -EINVAL;
 start = req.start;
 if (start & ~PAGE_MASK) return -EINVAL;
 length = PAGE_ALIGN((unsigned long)req.length);
 if (req.length && !length) return -EINVAL;
 end = start + length;
 if (end < start) return -EINVAL;
 if (!length) return 0;
 if (write_lock) down_write(&mm->mmap_sem);
 else down_read(&mm->mmap_sem);
 vma = find_vma(mm, start);
 while (start < end) {
  if (!vma) { error = -ENOMEM; break; }
  if (start < vma->vm_start) {
   hole = -ENOMEM; start = vma->vm_start;
   if (start >= end) break;
  }
  if (!write_lock && (vma->vm_flags & (VM_LOCKED | VM_HUGETLB | VM_PFNMAP))) {
   error = -EINVAL; break;
  }
  stop = min(end, vma->vm_end);
  if (!write_lock) zap_range(vma, start, stop - start, NULL);
  else {
   flags = vma->vm_flags & ~(VM_RAND_READ | VM_SEQ_READ);
   if (req.advice == MADV_RANDOM) flags |= VM_RAND_READ;
   if (req.advice == MADV_SEQUENTIAL) flags |= VM_SEQ_READ;
   if (flags != vma->vm_flags) {
    if (start != vma->vm_start) { error = split_range(mm, vma, start, 1); if (error) break; }
    if (stop != vma->vm_end) { error = split_range(mm, vma, stop, 0); if (error) break; }
    vma->vm_flags = flags;
   }
  }
  start = stop; vma = vma->vm_next;
 }
 if (write_lock) up_write(&mm->mmap_sem);
 else up_read(&mm->mmap_sem);
 return error ? error : hole;
}
static const struct file_operations operations = {
 .owner = THIS_MODULE, .unlocked_ioctl = discard_ioctl,
};
static struct miscdevice device = {
 .minor = MISC_DYNAMIC_MINOR, .name = "kobo_madvise", .fops = &operations, .mode = 0600,
};
static int __init kobo_vm_init(void)
{
 zap_range = (typeof(zap_range))kallsyms_lookup_name("zap_page_range");
 split_range = (typeof(split_range))kallsyms_lookup_name("split_vma");
 if (!zap_range || !split_range) return -ENOENT;
 return misc_register(&device);
}
static void __exit kobo_vm_exit(void) { misc_deregister(&device); }
module_init(kobo_vm_init);
module_exit(kobo_vm_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Opt-in MADV_DONTNEED helper for Kobo Android experiments");
