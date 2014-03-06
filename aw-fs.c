
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

#if __linux__ || __APPLE__
# include <sys/mman.h>
# include <sys/socket.h>
# include <fcntl.h>
# include <unistd.h>
#endif
#include <errno.h>
#include <stdlib.h>
#include "aw-fs.h"

int fs_stat(const char *path, fs_stat_t *st) {
#if _WIN32
	return _stat64(path, st);
#elif __linux__ || __APPLE__
	return stat(path, st);
#endif
}

size_t fs_stat_size(fs_stat_t *st) {
	return st->st_size;
}

void *fs_map(struct fs_map *map, const char *path) {
# if _WIN32
	LARGE_INTEGER size;

	if ((map->file = CreateFile(
			path, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL)) == INVALID_HANDLE_VALUE)
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

	map->addr = MapViewOfFile(map->mapping, FILE_MAP_READ, 0, 0, size.QuadPart);
	map->size = size;

	return map->addr;
# elif __linux__ || __APPLE__
	fs_stat_t st;
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
# endif
}

void fs_unmap(struct fs_map *map) {
# if _WIN32
	UnmapViewOfFile(map->addr);
	CloseHandle(map->mapping);
	CloseHandle(map->file);
# elif __linux__ || __APPLE__
	munmap(map->addr, map->size);
# endif
}

intptr_t fs_open(const char *path, int flags) {
#if _WIN32
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

	if ((flags & O_EXCL) != 0)
		creat = CREATE_NEW;

	return (intptr_t) CreateFile(
		path, oflag, share, NULL, creat, FILE_ATTRIBUTE_NORMAL, NULL);
#elif __linux__ || __APPLE__
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

	if ((flags & O_CREAT) != 0)
		return open(path, oflag | O_CREAT, 0644);

	return open(path, oflag);
#endif
}

void fs_close(intptr_t fd) {
#if _WIN32
	CloseHandle((HANDLE) fd);
#elif __linux__ || __APPLE__
	close(fd);
#endif
}

ssize_t fs_read(intptr_t fd, void *p, size_t n) {
#if _WIN32
	ssize_t off, len;

	for (off = 0, len = n; len != 0; off += len, len = n - off)
		if (!ReadFile((HANDLE) fd, (char *) p + off, len, &len, NULL))
			return -1;
		else if (len == 0)
			break;

	return off;
#elif __linux__ || __APPLE__
	ssize_t err, off, len;

	for (off = 0, len = n; len != 0; off += err > 0 ? err : 0, len = n - off)
		if ((err = read(fd, (char *) p + off, len)) == 0)
			break;
		else if (errno != EINTR)
			return -1;

	return off;
#endif
}

ssize_t fs_write(intptr_t fd, const void *p, size_t n) {
#if _WIN32
	ssize_t off, len;

	for (off = 0, len = n; len != 0; off += len, len = n - off)
		if (!WriteFile((HANDLE) fd, (const char *) p + off, len, &len, NULL))
			return -1;

	return -1;
#elif __linux__ || __APPLE__
	ssize_t err, off, len;

	for (off = 0, len = n; len != 0; off += err > 0 ? err : 0, len = n - off)
		if ((err = write(fd, (const char *) p + off, len)) < 0 && errno != EINTR)
			return -1;

	return off;
#endif
}

#if __linux__ || __APPLE__
ssize_t fs_sendfile(int sd, intptr_t fd, size_t n) {
	ssize_t err, off;
	off_t len;

	for (off = 0, len = n; len != 0; off += len, len = n - off)
		if ((err = sendfile(fd, sd, off, &len, NULL, 0)) < 0 && errno != EINTR)
			return -1;

	return off;
}
#endif

fs_dir_t *fs_firstdir(const char *path, fs_dirent_t *ent) {
#if _WIN32
	size_t size = strlen(path) + 3;
	char *buf = malloc(size);
	fs_dir_t *dir;

	snprintf(buf, size, "%s/*", path);
	dir = (fs_dir_t *) (_findfirst(buf, ent) + 1);

	free(buf);
	return dir;
#elif __linux__ || __APPLE__
	fs_dir_t *dir = opendir(path);
	fs_dirent_t *res;

	if (dir == NULL)
		return NULL;

	if (readdir_r(dir, ent, &res) < 0 || res != ent)
		return closedir(dir), NULL;

	return dir;
#endif
}

bool fs_nextdir(fs_dir_t *dir, fs_dirent_t *ent) {
#if _WIN32
	return _findnext((intptr_t) dir, ent) == 0;
#elif __linux__ || __APPLE__
	fs_dirent_t *res;
	return readdir_r(dir, ent, &res) == 0 && res == ent;
#endif
}

void fs_closedir(fs_dir_t *dir) {
#if _WIN32
	_findclose((intptr_t) dir);
#elif __linux__ || __APPLE__
	closedir(dir);
#endif
}

const char *fs_dirent_name(fs_dirent_t *ent) {
#if _WIN32
	return ent->name;
#elif __linux__ || __APPLE__
	return ent->d_name;
#endif
}

bool fs_dirent_isdir(fs_dirent_t *ent) {
#if _WIN32
	return ent->attrib == _A_SUBDIR;
#elif __linux__ || __APPLE__
	return ent->d_type == DT_DIR;
#endif
}

