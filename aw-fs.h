
/*
   Copyright (c) 2014-2021 Malte Hildingsson, malte (at) afterwi.se

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

#if !defined(_MSC_VER) || _MSC_VER >= 1800
# include <stdbool.h>
#endif
#if !defined(_MSC_VER) || _MSC_VER >= 1600
# include <stdint.h>
#endif
#include <stddef.h>

#if defined(__linux__) || defined(__APPLE__)
# include <dirent.h>
#endif

#if defined(__APPLE__)
# include <sys/mount.h>
# include <sys/vnode.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#if defined(_fs_dllexport)
# if defined(_MSC_VER)
#  define _fs_api extern __declspec(dllexport)
# elif defined(__GNUC__)
#  define _fs_api __attribute__((visibility("default"))) extern
# endif
#elif defined(_fs_dllimport)
# if defined(_MSC_VER)
#  define _fs_api extern __declspec(dllimport)
# endif
#endif
#ifndef _fs_api
# define _fs_api extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
# define FS_PATH_MAX (_MAX_PATH)
#elif defined(__linux__) || defined(__APPLE__)
# define FS_PATH_MAX (PATH_MAX)
#endif

#if defined(_MSC_VER)
typedef signed __int64 fs_ssize_t;
#else
typedef ssize_t fs_ssize_t;
#endif

#if defined(_WIN32)
typedef struct __stat64 fs_stat_t;
#elif defined(__linux__) || defined(__APPLE__)
typedef struct stat fs_stat_t;
#endif

#define FS_DIRENT_MAX (64)

#if defined(__APPLE__)
struct fs_attr {
	unsigned length;
	attrreference_t name;
	fsobj_type_t type;
	struct timespec mtime;
} __attribute__((aligned(4), packed));
#endif

typedef union {
#if defined(_WIN32)
	struct _finddata_t data[FS_DIRENT_MAX];
#elif defined(__linux__) || defined(__APPLE__)
# if defined(__APPLE__)
	char attrbuf[FS_DIRENT_MAX * (sizeof (struct fs_attr) + 64)];
# endif
	struct dirent dirent[FS_DIRENT_MAX];
#endif
} fs_dirbuf_t;

typedef struct {
#if defined(_WIN32)
	intptr_t dir;
	struct _finddata_t *data;
#elif defined(__linux__) || defined(__APPLE__)
	const char *path;
	int fd;
	DIR *dir;
	union {
		struct dirent *dirent;
# if defined(__APPLE__)
		struct fs_attr *attr;
# endif
	};
# if defined(__APPLE__)
	struct attrlist attrlist;
# endif
#endif
	unsigned count;
} fs_dir_t;

/* status */

_fs_api int fs_stat(const char *path, fs_stat_t *st);

/* memory-mapping */

struct fs_map {
	void *addr;
	size_t size;
# if _WIN32
	HANDLE file;
	HANDLE mapping;
# endif
};

_fs_api void *fs_map(struct fs_map *map, const char *path);
_fs_api void fs_unmap(struct fs_map *map);

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

_fs_api intptr_t fs_open(const char *path, int flags);
_fs_api void fs_close(intptr_t fd);

enum {
	FS_LOCK_SHARE = 0x0,
	FS_LOCK_EXCL = 0x1,
	FS_LOCK_NOWAIT = 0x2,
	FS_LOCK_UNLOCK = 0x4
};

_fs_api int fs_lock(intptr_t fd, int flags);

_fs_api int fs_truncate(intptr_t fd, size_t n);

enum {
	FS_SEEK_SET,
	FS_SEEK_CUR,
	FS_SEEK_END
};

_fs_api off_t fs_seek(intptr_t fd, off_t off, int whence);

_fs_api fs_ssize_t fs_read(intptr_t fd, void *p, size_t n);
_fs_api fs_ssize_t fs_write(intptr_t fd, const void *p, size_t n);
_fs_api fs_ssize_t fs_sendfile(int sd, intptr_t fd, size_t n);

/* dir ops */

_fs_api char *fs_getcwd(char *buf, size_t size);

_fs_api bool fs_opendirwalk(fs_dir_t *dir, fs_dirbuf_t *buf, const char *path);
_fs_api bool fs_bufferdirwalk(fs_dir_t *dir, fs_dirbuf_t *buf);
_fs_api void fs_closedirwalk(fs_dir_t *dir);

_fs_api bool fs_nextdirent(const char **name, int *isdir, time_t *mtime, fs_dir_t *dir);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AW_FS_H */

