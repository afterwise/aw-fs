
#include "aw-fs.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char *argv[]) {
	fs_stat_t st;
	fs_dir_t dir;
	fs_dirbuf_t buf;
	struct fs_map map;
	char *text;
	char *text2;
	size_t size;
	intptr_t fd;
	const char *name;
	int isdir;
	time_t mtime;

	(void) argc;
	(void) argv;

	fs_stat("test.c", &st);
	printf("stat test.c -> size=%ld\n", (unsigned long) st.st_size);

	size = st.st_size;
	text = malloc(size + 1);
	text2 = malloc(size + 1);

	fs_map(&map, "test.c");
	memcpy(text, map.addr, size);
	text[size] = 0;
	printf("<<<%s>>>\n", text);
	fs_unmap(&map);

	fd = fs_open("test.c", FS_RDONLY);
	fs_read(fd, text2, size);
	text2[size] = 0;
	fs_close(fd);

	assert(strcmp(text, text2) == 0);

	free(text);
	free(text2);

	if (fs_opendirwalk(&dir, &buf, "..")) {
		do {
			while (fs_nextdirent(&name, &isdir, &mtime, &dir))
				printf(
					"dir .. -> %s | dir? %s | mtime: %s", name,
					isdir ? "Y" : "N", ctime(&mtime));
		} while (fs_bufferdirwalk(&dir, &buf));

		fs_closedirwalk(&dir);
	}

	return 0;
}

