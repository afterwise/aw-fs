
export CFLAGS += -Wall -Wextra

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

