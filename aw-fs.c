
/*
   Copyright (c) 2014-2025 Malte Hildingsson, malte (at) afterwi.se

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

#ifndef _fs_nofeatures
# if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN 1
# elif defined(__linux__)
#  define _BSD_SOURCE 1
#  define _DEFAULT_SOURCE 1
#  define _POSIX_C_SOURCE 200809L
#  define _SVID_SOURCE 1
# elif defined(__APPLE__)
#  define _DARWIN_C_SOURCE 1
# endif
#endif /* _fs_nofeatures */

#include "aw-fs.h"

#if defined(_WIN32)
# include <direct.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
# include <fcntl.h>
# include <sys/mman.h>
# include <unistd.h>
#endif

#if defined(_WIN32)
# include <malloc.h>
#elif defined(__SCE__)
# include <stdlib.h>
#else
# include <alloca.h>
#endif

#if defined(__SCE__)
# include <kernel.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>

int fs_stat(const char *path, fs_stat_t *st) {
#if defined(_WIN32)
	return _stat64(path, st);
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	return stat(path, st);
#endif
}

void *fs_map(struct fs_map *map, const char *path) {
#if defined(_WIN32)
	LARGE_INTEGER size;

	if ((map->file = CreateFileA(
			path, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
		return NULL;

	if (!GetFileSizeEx(map->file, &size)) {
		CloseHandle(map->file);
		return NULL;
	}

	if ((map->mapping = CreateFileMapping(
			map->file, NULL, PAGE_READONLY, 0, 0, NULL)) == INVALID_HANDLE_VALUE) {
		CloseHandle(map->file);
		return NULL;
	}

	map->addr = MapViewOfFile(map->mapping, FILE_MAP_READ, 0, 0, (size_t) size.QuadPart);
	map->size = (size_t) size.QuadPart;

	return map->addr;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	struct stat st;
	void *addr;
	int fd;

	if ((fd = open(path, O_RDONLY)) < 0)
		return NULL;

	if (fstat(fd, &st) < 0)
		return close(fd), NULL;

	if ((addr = mmap(NULL, st.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0)) == MAP_FAILED)
		return close(fd), NULL;

	close(fd);

	map->addr = addr;
	map->size = st.st_size;

	return addr;
#endif
}

void fs_unmap(struct fs_map *map) {
#if defined(_WIN32)
	UnmapViewOfFile(map->addr);
	CloseHandle(map->mapping);
	CloseHandle(map->file);
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	munmap(map->addr, map->size);
#endif
}

intptr_t fs_open(const char *path, int flags) {
#if defined(_WIN32)
	HANDLE fd;
	int oflag = GENERIC_READ;
	int creat = OPEN_EXISTING;
	int share = FILE_SHARE_READ | FILE_SHARE_DELETE;

	if ((flags & FS_RDWR) != 0) {
		oflag |= GENERIC_WRITE;
		share = 0;
	}

	if ((flags & FS_WRONLY) != 0) {
		oflag = GENERIC_WRITE;
		share = 0;
	}

	if ((flags & FS_APPEND) != 0) {
		oflag &= ~FILE_WRITE_DATA;
		oflag |= FILE_APPEND_DATA;
	}

	if ((flags & FS_CREAT) != 0)
		creat = OPEN_ALWAYS;

	if ((flags & FS_TRUNC) != 0)
		creat = CREATE_ALWAYS;

	if ((flags & FS_EXCL) != 0)
		creat = CREATE_NEW;

	if ((fd = CreateFileA(
			path, oflag, share, NULL, creat,
			FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
		return -1;

	return (intptr_t) fd;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	int oflag = O_RDONLY;

	if ((flags & FS_RDWR) != 0)
		oflag = O_RDWR;

	if ((flags & FS_WRONLY) != 0)
		oflag = O_WRONLY;

	if ((flags & FS_APPEND) != 0)
		oflag |= O_APPEND;

	if ((flags & FS_TRUNC) != 0)
		oflag |= O_TRUNC;

	if ((flags & FS_EXCL) != 0)
		oflag |= O_EXCL;

	if ((flags & FS_CREAT) != 0)
		return open(path, oflag | O_CREAT, 0644);

	return open(path, oflag);
#endif
}

void fs_close(intptr_t fd) {
#if defined(_WIN32)
	CloseHandle((HANDLE) fd);
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	close(fd);
#endif
}

bool fs_remove(const char* path) {
#if defined(_WIN32)
	return !!DeleteFileA(path);
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	return unlink(path) == 0;
#endif
}

int fs_lock(intptr_t fd, int flags) {
#if defined(_WIN32)
	OVERLAPPED ol;
	memset(&ol, 0, sizeof ol);

	if ((flags & FS_LOCK_UNLOCK) == 0) {
		int lflag = 0;

		if ((flags & FS_LOCK_EXCL) != 0)
			lflag |= LOCKFILE_EXCLUSIVE_LOCK;

		if ((flags & FS_LOCK_NOWAIT) != 0)
			lflag |= LOCKFILE_FAIL_IMMEDIATELY;

		if (!LockFileEx((HANDLE) fd, lflag, 0, MAXDWORD, MAXDWORD, &ol))
			return -1;
	} else if (!UnlockFileEx((HANDLE) fd, 0, MAXDWORD, MAXDWORD, &ol))
		return -1;

	return 0;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	struct flock fl;

	if ((flags & FS_LOCK_UNLOCK) != 0)
		fl.l_type = F_UNLCK;
	else if ((flags & FS_LOCK_EXCL) != 0)
		fl.l_type = F_WRLCK;
	else
		fl.l_type = F_RDLCK;

	fl.l_start = 0;
	fl.l_len = 0;
	fl.l_pid = getpid();

	if ((flags & (FS_LOCK_NOWAIT | FS_LOCK_UNLOCK)) == 0)
		return fcntl(fd, F_SETLKW, &fl);
	else
		return fcntl(fd, F_SETLK, &fl);
#endif
}

int fs_truncate(intptr_t fd, size_t n) {
#if defined(_WIN32)
	LARGE_INTEGER ln;
	ln.QuadPart = n;
	if (SetFilePointerEx((HANDLE) fd, ln, NULL, FILE_BEGIN))
		if (SetEndOfFile((HANDLE) fd))
			return 0;

	return -1;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	return ftruncate(fd, n);
#endif
}

off_t fs_seek(intptr_t fd, off_t off, int whence) {
#if defined(_WIN32)
	LARGE_INTEGER loff;
	loff.QuadPart = off;
#endif

	switch (whence) {
	case FS_SEEK_SET:
#if defined(_WIN32)
		whence = FILE_BEGIN;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
		whence = SEEK_SET;
#endif
		break;

	case FS_SEEK_CUR:
#if defined(_WIN32)
		whence = FILE_CURRENT;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
		whence = SEEK_CUR;
#endif
		break;

	case FS_SEEK_END:
#if defined(_WIN32)
		whence = FILE_END;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
		whence = SEEK_END;
#endif
		break;
	}

#if defined(_WIN32)
	return SetFilePointerEx((HANDLE) fd, loff, &loff, whence) ? (off_t) loff.QuadPart : -1;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	return lseek(fd, off, whence);
#endif
}

fs_ssize_t fs_read(intptr_t fd, void *p, size_t n) {
#if defined(_WIN32)
	fs_ssize_t off;
	DWORD len;

	for (off = 0, len = (DWORD) n; len != 0; off += len, len = (DWORD) (n - off))
		if (!ReadFile((HANDLE) fd, (char *) p + off, len, &len, NULL))
			return -1;
		else if (len == 0)
			break;

	return off;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	ssize_t err, off, len;

	for (off = 0, len = n; len != 0; off += err > 0 ? err : 0, len = n - off)
		if ((err = read(fd, (char *) p + off, len)) == 0)
			break;
		else if (err < 0)
			return -1;

	return off;
#endif
}

fs_ssize_t fs_write(intptr_t fd, const void *p, size_t n) {
#if defined(_WIN32)
	fs_ssize_t off;
	DWORD len;

	for (off = 0, len = (DWORD) n; len != 0; off += len, len = (DWORD) (n - off))
		if (!WriteFile((HANDLE) fd, (const char *) p + off, len, &len, NULL))
			return -1;

	return off;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
	ssize_t err, off, len;

	for (off = 0, len = n; len != 0; off += err > 0 ? err : 0, len = n - off)
		if ((err = write(fd, (const char *) p + off, len)) < 0)
			return -1;

	return off;
#endif
}

char *fs_getcwd(char *buf, size_t size) {
#if defined(_WIN32)
	return _getcwd(buf, (int) size);
#elif defined(__linux__) || defined(__APPLE__)
	return getcwd(buf, size);
#elif defined(__SCE__)
	strncpy_s(buf, size, "", 0);
	return buf;
#endif
}

bool fs_opendirwalk(fs_dir_t *dir, fs_dirbuf_t *buf, const char *path) {
	memset(dir, 0, sizeof *dir);
	dir->path = path;

#if defined(_WIN32)
	size_t np = strlen(path) + 3;
	char *p = (char *) alloca(np);
	unsigned n;

	snprintf(p, np, "%s/*", path);

	if ((dir->dir = _findfirst(p, &buf->data[0])) < 0)
		return false;

	for (n = 1; n < FS_DIRENT_MAX; ++n)
		if (_findnext(dir->dir, &buf->data[n]) < 0)
			break;

	dir->data = buf->data;
	dir->count = n;

	return true;
#elif defined(__linux__) || defined(__APPLE__)
	if ((dir->dir = opendir(path)) == NULL)
		return false;

	if (fs_bufferdirwalk(dir, buf))
		return true;

	fs_closedirwalk(dir);
	return false;
#elif defined(__SCE__)
	if ((dir->fd = sceKernelOpen(path, SCE_KERNEL_O_DIRECTORY, 0)) < 0)
		return false;

	if (fs_bufferdirwalk(dir, buf))
		return true;

	fs_closedirwalk(dir);
	return false;
#endif
}

#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

bool fs_bufferdirwalk(fs_dir_t *dir, fs_dirbuf_t *buf) {
	int n;

	if (dir->count > 0 && dir->count < FS_DIRENT_MAX)
		return false;

#if defined(_WIN32)
	for (n = 0; n < FS_DIRENT_MAX; ++n)
		if (_findnext(dir->dir, &buf->data[n]) < 0)
			break;

	dir->data = buf->data;
	dir->count = n;
#elif defined(__linux__) || defined(__APPLE__)
	struct dirent *res;
	for (n = 0; n < FS_DIRENT_MAX; ++n)
		if (readdir_r(dir->dir, &buf->dirent[n], &res) != 0 ||
				res != &buf->dirent[n])
			break;

	dir->dirent = buf->dirent;
	dir->count = n;
#elif defined(__SCE__)
	n = sceKernelGetdents(dir->fd, (void*) buf->dirent, (int) sizeof buf->dirent);
	if (n > 0)
		n /= (int) sizeof buf->dirent[0];

	dir->dirent = buf->dirent;
	dir->count = n;
#endif

	return n > 0;
}

#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

void fs_closedirwalk(fs_dir_t *dir) {
#if defined(_WIN32)
	_findclose(dir->dir);
#elif defined(__linux__) || defined(__APPLE__)
	closedir(dir->dir);
#elif defined(__SCE__)
	sceKernelClose(dir->fd);
#endif
}

#if defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
static int statdirent(struct stat *st, const char *dir, const char *ent) {
	size_t np = strlen(dir) + strlen(ent) + 2;
	char *p = alloca(np);

	snprintf(p, np, "%s/%s", dir, ent);
	return stat(p, st);
}
#endif

#if defined(_WIN32)
static void nextdata(const char **name, int *isdir, time_t *mtime, fs_dir_t *dir) {
	struct _finddata_t *data = dir->data;

	if (name != NULL)
		*name = data->name;

	if (isdir != NULL)
		*isdir = (data->attrib & _A_SUBDIR) != 0;

	if (mtime != NULL)
		*mtime = data->time_write;

	dir->data = &dir->data[1];
}
#endif

#if defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
static void nextdirent(const char **name, int *isdir, time_t *mtime, fs_dir_t *dir) {
	if (name != NULL)
		*name = dir->dirent->d_name;

	if (isdir != NULL)
		*isdir = (dir->dirent->d_type == DT_DIR);

	if (mtime != NULL) {
		struct stat st;

		statdirent(&st, dir->path, dir->dirent->d_name);
# if defined(__APPLE__)
		*mtime = st.st_mtimespec.tv_sec;
# else
		*mtime = st.st_mtime;
# endif
	}

	dir->dirent = &dir->dirent[1];
}
#endif

bool fs_nextdirent(const char **name, int *isdir, time_t *mtime, fs_dir_t *dir) {
	if (dir->count > 0) {
#if defined(_WIN32)
		nextdata(name, isdir, mtime, dir);
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__)
		nextdirent(name, isdir, mtime, dir);
#endif

		dir->count--;
		return true;
	}

	return false;
}

