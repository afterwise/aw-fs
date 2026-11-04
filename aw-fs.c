
/*
   Copyright (c) 2014-2026 Malte Hildingsson, malte (at) afterwi.se

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
#  ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN 1
#  endif
# elif defined(__linux__) || defined(__NINTENDO__)
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
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
# include <fcntl.h>
# include <sys/mman.h>
# include <unistd.h>
#endif

#if defined(_WIN32)
# include <malloc.h>
#elif !defined(__SCE__)
# include <alloca.h>
#endif

#if defined(__SCE__)
# include <kernel.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
/* paths are UTF-8; widen allocates room for extra characters after the terminator */
static wchar_t *widen(const char *s, size_t extra) {
	int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
	wchar_t *w;

	if (n <= 0 || (w = (wchar_t *) malloc((n + extra) * sizeof (wchar_t))) == NULL)
		return NULL;

	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, w, n);
	return w;
}

static char *narrow(const wchar_t *w) {
	int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
	char *s;

	if (n <= 0 || (s = (char *) malloc(n)) == NULL)
		return NULL;

	WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, NULL, NULL);
	return s;
}
#endif

int fs_stat(const char *path, fs_stat_t *st) {
#if defined(_WIN32)
	wchar_t *tmp = widen(path, 0);
	int err;
	if (tmp == NULL)
		return -1;
	err = _wstat64(tmp, st);
	free(tmp);
	return err;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
	return stat(path, st);
#endif
}

void *fs_map(struct fs_map *map, const char *path) {
#if defined(_WIN32)
	LARGE_INTEGER size;
	wchar_t *tmp = widen(path, 0);

	memset(map, 0, sizeof *map);

	if (tmp == NULL)
		return NULL;

	map->file = CreateFileW(
		tmp, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	free(tmp);

	if (map->file == INVALID_HANDLE_VALUE) {
		map->file = NULL;
		return NULL;
	}

	/* empty files cannot be mapped */
	if (!GetFileSizeEx(map->file, &size) ||
			(map->mapping = CreateFileMapping(map->file, NULL, PAGE_READONLY, 0, 0, NULL)) == NULL ||
			(map->addr = MapViewOfFile(map->mapping, FILE_MAP_READ, 0, 0, (size_t) size.QuadPart)) == NULL) {
		fs_unmap(map);
		return NULL;
	}

	map->size = (size_t) size.QuadPart;
	return map->addr;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
	struct stat st;
	void *addr;
	int fd;

	memset(map, 0, sizeof *map);

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
	if (map->addr != NULL)
		UnmapViewOfFile(map->addr);
	if (map->mapping != NULL)
		CloseHandle(map->mapping);
	if (map->file != NULL)
		CloseHandle(map->file);
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
	if (map->addr != NULL)
		munmap(map->addr, map->size);
#endif
	memset(map, 0, sizeof *map);
}

intptr_t fs_open(const char *path, int flags) {
#if defined(_WIN32)
	HANDLE fd;
	int oflag = GENERIC_READ;
	int creat = OPEN_EXISTING;
	int share = FILE_SHARE_READ | FILE_SHARE_DELETE;
	wchar_t *tmp = widen(path, 0);

	if (tmp == NULL)
		return -1;

	if ((flags & FS_RDWR) != 0) {
		oflag |= GENERIC_WRITE;
		share = 0;
	}

	if ((flags & FS_WRONLY) != 0) {
		oflag = GENERIC_WRITE;
		share = 0;
	}

	/* GENERIC_WRITE includes FILE_WRITE_DATA, which would write at the file position */
	if ((flags & FS_APPEND) != 0 && (oflag & GENERIC_WRITE) != 0) {
		oflag &= ~GENERIC_WRITE;
		oflag |= FILE_GENERIC_WRITE & ~FILE_WRITE_DATA;
	}

	if ((flags & FS_CREAT) != 0)
		creat = OPEN_ALWAYS;

	if ((flags & FS_TRUNC) != 0)
		creat = (flags & FS_CREAT) != 0 ? CREATE_ALWAYS : TRUNCATE_EXISTING;

	if ((flags & FS_EXCL) != 0)
		creat = CREATE_NEW;

	if ((fd = CreateFileW(
			tmp, oflag, share, NULL, creat,
			FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
		free(tmp);
		return -1;
	}

	free(tmp);
	return (intptr_t) fd;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
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
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
	close(fd);
#endif
}

bool fs_remove(const char* path) {
#if defined(_WIN32)
	wchar_t *tmp = widen(path, 0);
	bool ok = tmp != NULL && DeleteFileW(tmp);
	free(tmp);
	return ok;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
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
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
	struct flock fl;

	memset(&fl, 0, sizeof fl);

	if ((flags & FS_LOCK_UNLOCK) != 0)
		fl.l_type = F_UNLCK;
	else if ((flags & FS_LOCK_EXCL) != 0)
		fl.l_type = F_WRLCK;
	else
		fl.l_type = F_RDLCK;

	fl.l_whence = SEEK_SET;
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
	/* unlike SetEndOfFile, this leaves the file position alone, as ftruncate does */
	FILE_END_OF_FILE_INFO info;
	info.EndOfFile.QuadPart = (LONGLONG) n;
	return SetFileInformationByHandle((HANDLE) fd, FileEndOfFileInfo, &info, sizeof info) ? 0 : -1;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
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
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
		whence = SEEK_SET;
#endif
		break;

	case FS_SEEK_CUR:
#if defined(_WIN32)
		whence = FILE_CURRENT;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
		whence = SEEK_CUR;
#endif
		break;

	case FS_SEEK_END:
#if defined(_WIN32)
		whence = FILE_END;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
		whence = SEEK_END;
#endif
		break;
	}

#if defined(_WIN32)
	return SetFilePointerEx((HANDLE) fd, loff, &loff, whence) ? (off_t) loff.QuadPart : -1;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
	return lseek(fd, off, whence);
#endif
}

/* ReadFile and WriteFile take 32-bit sizes */
#define FS_IO_CHUNK ((size_t) 1 << 30)

fs_ssize_t fs_read(intptr_t fd, void *p, size_t n) {
#if defined(_WIN32)
	size_t off;
	DWORD len;

	for (off = 0; off < n; off += len)
		if (!ReadFile((HANDLE) fd, (char *) p + off, (DWORD) (n - off < FS_IO_CHUNK ? n - off : FS_IO_CHUNK), &len, NULL))
			return -1;
		else if (len == 0)
			break;

	return (fs_ssize_t) off;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
	ssize_t err, off, len;

	for (off = 0, len = n; len != 0; off += err > 0 ? err : 0, len = n - off)
		if ((err = read(fd, (char *) p + off, len)) == 0)
			break;
		else if (err < 0 && errno != EINTR)
			return -1;

	return off;
#endif
}

fs_ssize_t fs_write(intptr_t fd, const void *p, size_t n) {
#if defined(_WIN32)
	size_t off;
	DWORD len;

	for (off = 0; off < n; off += len)
		if (!WriteFile((HANDLE) fd, (const char *) p + off, (DWORD) (n - off < FS_IO_CHUNK ? n - off : FS_IO_CHUNK), &len, NULL))
			return -1;
		else if (len == 0)
			break;

	return (fs_ssize_t) off;
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
	ssize_t err, off, len;

	for (off = 0, len = n; len != 0; off += err > 0 ? err : 0, len = n - off)
		if ((err = write(fd, (const char *) p + off, len)) < 0 && errno != EINTR)
			return -1;

	return off;
#endif
}

char *fs_getcwd(char *buf, size_t size) {
#if defined(_WIN32)
	wchar_t *tmp = _wgetcwd(NULL, 0);
	char *res = NULL;
	if (tmp != NULL && WideCharToMultiByte(CP_UTF8, 0, tmp, -1, buf, (int) size, NULL, NULL) > 0)
		res = buf;
	free(tmp);
	return res;
#elif defined(__linux__) || defined(__APPLE__) || defined(__NINTENDO__)
	return getcwd(buf, size);
#elif defined(__SCE__)
	strcpy_s(buf, size, "");
	return buf;
#endif
}

bool fs_opendirwalk(fs_dir_t *dir, fs_dirbuf_t *buf, const char *path) {
	memset(dir, 0, sizeof *dir);
	dir->path = path;

#if defined(_WIN32)
	unsigned n;
	wchar_t *tmp = widen(path, 2);

	if (tmp == NULL)
		return false;
	wcscat_s(tmp, wcslen(tmp) + 3, L"/*");

	dir->cur = NULL;
	dir->dir = _wfindfirst(tmp, &buf->data[0]);
	free(tmp);

	if (dir->dir < 0)
		return false;

	for (n = 1; n < FS_DIRENT_MAX; ++n)
		if (_wfindnext(dir->dir, &buf->data[n]) < 0)
			break;

	dir->data = buf->data;
	dir->count = n;

	return true;
#elif defined(__linux__) || defined(__APPLE__) || defined(__NINTENDO__)
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
		if (_wfindnext(dir->dir, &buf->data[n]) < 0)
			break;

	dir->data = buf->data;
	dir->count = n;
#elif defined(__linux__) || defined(__APPLE__) || defined(__NINTENDO__)
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
	if (dir->cur != NULL)
		free(dir->cur);
	_findclose(dir->dir);
#elif defined(__linux__) || defined(__APPLE__) || defined(__NINTENDO__)
	closedir(dir->dir);
#elif defined(__SCE__)
	sceKernelClose(dir->fd);
#endif
}

#if defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
static int statdirent(struct stat *st, const char *dir, const char *ent) {
	size_t np = strlen(dir) + strlen(ent) + 2;
	char *p = alloca(np);

	snprintf(p, np, "%s/%s", dir, ent);
	return stat(p, st);
}
#endif

#if defined(_WIN32)
static void nextdata(const char **name, int *isdir, time_t *mtime, fs_dir_t *dir) {
	struct _wfinddata_t *data = dir->data;

	if (name != NULL) {
		if (dir->cur != NULL)
			free(dir->cur);
		dir->cur = narrow(data->name);
		*name = dir->cur != NULL ? dir->cur : "";
	}

	if (isdir != NULL)
		*isdir = (data->attrib & _A_SUBDIR) != 0;

	if (mtime != NULL)
		*mtime = data->time_write;

	dir->data = &dir->data[1];
}
#endif

#if defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
static void nextdirent(const char **name, int *isdir, time_t *mtime, fs_dir_t *dir) {
	if (name != NULL)
		*name = dir->dirent->d_name;

	if (isdir != NULL) {
		struct stat st;

		/* not every file system reports the type */
		if (dir->dirent->d_type == DT_UNKNOWN)
			*isdir = statdirent(&st, dir->path, dir->dirent->d_name) == 0 && S_ISDIR(st.st_mode);
		else
			*isdir = (dir->dirent->d_type == DT_DIR);
	}

	if (mtime != NULL) {
		struct stat st;

		if (statdirent(&st, dir->path, dir->dirent->d_name) < 0)
			*mtime = 0;
		else
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
#elif defined(__linux__) || defined(__APPLE__) || defined(__SCE__) || defined(__NINTENDO__)
		nextdirent(name, isdir, mtime, dir);
#endif

		dir->count--;
		return true;
	}

	return false;
}

