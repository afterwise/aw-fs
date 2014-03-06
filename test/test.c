
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aw-fs.h"

int main(int argc, char *argv[]) {
	fs_stat_t st;
	fs_dir_t *dir;
	fs_dirent_t ent;
	struct fs_map map;
	char *text;
	char *text2;
	size_t size;
	intptr_t fd;

	(void) argc;
	(void) argv;

	fs_stat("test.c", &st);
	printf("stat test.c -> size=%zu\n", fs_stat_size(&st));

	size = fs_stat_size(&st);
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

	dir = fs_firstdir("..", &ent);

	do {
		printf(
			"dir .. -> %s (dir? %s)\n", fs_dirent_name(&ent),
			fs_dirent_isdir(&ent) ? "Y" : "N");
	} while (fs_nextdir(dir, &ent));

	fs_closedir(dir);

	return 0;
}

