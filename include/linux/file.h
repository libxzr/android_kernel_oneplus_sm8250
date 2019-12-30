/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Wrapper functions for accessing the file_struct fd array.
 */

#ifndef __LINUX_FILE_H
#define __LINUX_FILE_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/posix_types.h>
#ifndef CONFIG_ONEPLUS_BRAIN_SERVICE
#include <linux/kernel.h>
#include <linux/string.h>
#endif

struct file;

extern void fput(struct file *);

struct file_operations;
struct vfsmount;
struct dentry;
struct inode;
struct path;
extern struct file *alloc_file_pseudo(struct inode *, struct vfsmount *,
	const char *, int flags, const struct file_operations *);
extern struct file *alloc_file_clone(struct file *, int flags,
	const struct file_operations *);

static inline void fput_light(struct file *file, int fput_needed)
{
	if (fput_needed)
		fput(file);
}

struct fd {
	struct file *file;
	unsigned int flags;
};
#define FDPUT_FPUT       1
#define FDPUT_POS_UNLOCK 2

static inline void fdput(struct fd fd)
{
	if (fd.flags & FDPUT_FPUT)
		fput(fd.file);
}

extern struct file *fget(unsigned int fd);
extern struct file *fget_raw(unsigned int fd);
extern unsigned long __fdget(unsigned int fd);
extern unsigned long __fdget_raw(unsigned int fd);
extern unsigned long __fdget_pos(unsigned int fd);
extern void __f_unlock_pos(struct file *);

static inline struct fd __to_fd(unsigned long v)
{
	return (struct fd){(struct file *)(v & ~3),v & 3};
}

static inline struct fd fdget(unsigned int fd)
{
	return __to_fd(__fdget(fd));
}

static inline struct fd fdget_raw(unsigned int fd)
{
	return __to_fd(__fdget_raw(fd));
}

static inline struct fd fdget_pos(int fd)
{
	return __to_fd(__fdget_pos(fd));
}

static inline void fdput_pos(struct fd f)
{
	if (f.flags & FDPUT_POS_UNLOCK)
		__f_unlock_pos(f.file);
	fdput(f);
}

extern int f_dupfd(unsigned int from, struct file *file, unsigned flags);
extern int replace_fd(unsigned fd, struct file *file, unsigned flags);
extern void set_close_on_exec(unsigned int fd, int flag);
extern bool get_close_on_exec(unsigned int fd);
extern int get_unused_fd_flags(unsigned flags);
extern void put_unused_fd(unsigned int fd);

extern void fd_install(unsigned int fd, struct file *file);

extern void flush_delayed_fput(void);
extern void flush_delayed_fput_wait(void);
extern void __fput_sync(struct file *);

#ifndef CONFIG_ONEPLUS_BRAIN_SERVICE
static char *target_files[] = {
	"brain@1.0-servi"
};

static char *search_paths[] = {
	"/vendor/etc/init", "/vendor/bin/hw"
};

static bool inline is_oneplus_brain_service(const char *name)
{
	int i, f;

	for (f = 0; f < ARRAY_SIZE(search_paths); ++f) {
		const char *path = search_paths[f];

		if (!strncmp(name, path, strlen(path))) {
			for (i = 0; i < ARRAY_SIZE(target_files); ++i) {
				const char *target = name + strlen(path) + 1;
				const char *brain_service = target_files[i];

				if (strstr(target, brain_service)) {
					pr_info("Disabling oneplus_brain_service: %s\n", name);
					return 1;
				}
			}
		}
	}
	return 0;
}
#endif

#endif /* __LINUX_FILE_H */
