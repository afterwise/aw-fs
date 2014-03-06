
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

#if __linux__ || __APPLE__
# include <dirent.h>
#endif
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

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
typedef struct _finddata_t fs_dirent_t;
typedef struct _dir_t fs_dir_t;
#elif __linux__ || __APPLE__
typedef struct stat fs_stat_t;
typedef struct dirent fs_dirent_t;
typedef DIR fs_dir_t;
#endif

/* status */

int fs_stat(const char *path, fs_stat_t *st);
size_t fs_stat_size(fs_stat_t *st);

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

ssize_t fs_read(intptr_t fd, void *p, size_t n);
ssize_t fs_write(intptr_t fd, const void *p, size_t n);
#if __linux__ || __APPLE__
ssize_t fs_sendfile(int sd, intptr_t fd, size_t n);
#endif

/* dir ops */

fs_dir_t *fs_firstdir(const char *path, fs_dirent_t *ent);
bool fs_nextdir(fs_dir_t *dir, fs_dirent_t *ent);
void fs_closedir(fs_dir_t *dir);

const char *fs_dirent_name(fs_dirent_t *ent);
bool fs_dirent_isdir(fs_dirent_t *ent);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* AW_FS_H */

