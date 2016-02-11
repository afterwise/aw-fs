
export CFLAGS += -std=c99 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -D_DARWIN_C_SOURCE -Wall -Wextra

test: test.o ../libaw-fs.a
	$(CC) $(LDFLAGS) -o $@ $^
#	$(CC) $(LDFLAGS) -o $@ $^ -lmswsock

%.o: %.c
	$(CC) $(CFLAGS) -I.. -c $< -o $@

../libaw-fs.a:
	$(MAKE) -C..

.PHONY: clean
clean:
	rm -f test test.o

