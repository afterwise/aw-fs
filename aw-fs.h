
/*
   Copyright (c) 2014 Malte Hildingsson, malte (at) afterwi.se

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
   THE SOFTWARE.
 */

#ifndef AW_FS_H
#define AW_FS_H

#if _WIN32
# include <windows.h>
# include <IO.h>
#endif

#if !_MSC_VER
# include <stdbool.h>
# include <stdint.h>
#endif

#if __linux__ || __APPLE__
# include <dirent.h>
#endif

#if __APPLE__
# include <sys/mount.h>
# include <sys/vnode.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#if _WIN32
# define FS_PATH_MAX (_MAX_PATH)
#elif __linux__ || __APPLE__
# define FS_PATH_MAX (PATH_MAX)
#endif

#if _WIN32
typedef struct __stat64 fs_stat_t;
#elif __linux__ || __APPLE__
typedef struct stat fs_stat_t;
#endif

#define FS_DIRENT_MAX (64)

#if __APPLE__
struct fs_attr {
	unsigned length;
	attrreference_t name;
	fsobj_type_t type;
	struct timespec mtime;
} __attribute__((aligned(4), packed));
#endif

typedef union {
#if _WIN32
	struct _finddata_t data[FS_DIRENT_MAX];
#elif __linux__ || __APPLE__
# if __APPLE__
	char attrbuf[FS_DIRENT_MAX * (sizeof (struct fs_attr) + 64)];
# endif
	struct dirent dirent[FS_DIRENT_MAX];
#endif
} fs_dirbuf_t;

typedef struct {
#if _WIN32
	intptr_t dir;
	struct _finddata_t *data;
#elif __linux__ || __APPLE__
	const char *path;
	int fd;
	DIR *dir;
	union {
		struct dirent *dirent;
# if __APPLE__
		struct fs_attr *attr;
# endif
	};
# if __APPLE__
	struct attrlist attrlist;
# endif
#endif
	unsigned count;
} fs_dir_t;

/* status */

int fs_stat(const char *path, fs_stat_t *st);

/* memory-mapping */

struct fs_map {
	void *addr;
	size_t size;
# if _WIN32
	HANDLE file;
	HANDLE mapping;
# endif
};

void *fs_map(struct fs_map *map, const char *path);
void fs_unmap(struct fs_map *map);

/* input/output */

enum {
	FS_RDONLY = 0x0,
	FS_WRONLY = 0x1,
	FS_RDWR = 0x2,
	FS_APPEND = 0x4,
	FS_TRUNC = 0x8,
	FS_CREAT = 0x10,
	FS_EXCL = 0x20
};

intptr_t fs_open(const char *path, int flags);
void fs_close(intptr_t fd);

enum {
	FS_LOCK_SHARE = 0x0,
	FS_LOCK_EXCL = 0x1,
	FS_LOCK_NOWAIT = 0x2,
	FS_LOCK_UNLOCK = 0x4
};

int fs_lock(intptr_t fd, int flags);

int fs_truncate(intptr_t fd, size_t n);

enum {
	FS_SEEK_SET,
	FS_SEEK_CUR,
	FS_SEEK_END
};

off_t fs_seek(intptr_t fd, off_t off, int whence);

ssize_t fs_read(intptr_t fd, void *p, size_t n);
ssize_t fs_write(intptr_t fd, const void *p, size_t n);
ssize_t fs_sendfile(int sd, intptr_t fd, size_t n);

/* dir ops */

char *fs_getcwd(char *buf, size_t size);

bool fs_opendirwalk(fs_dir_t *dir, fs_dirbuf_t *buf, const char *path);
bool fs_bufferdirwalk(fs_dir_t *dir, fs_dirbuf_t *buf);
void fs_closedirwalk(fs_dir_t *dir);

bool fs_nextdirent(const char **name, int *isdir, time_t *mtime, fs_dir_t *dir);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AW_FS_H */

