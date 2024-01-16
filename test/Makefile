
export CFLAGS += -std=c99 -Wall -Wextra

test: test.o ../libaw-fs.a
	$(CC) $(LDFLAGS) -o $@ $^
#	$(CC) $(LDFLAGS) -o $@ $^ -lmswsock

%.o: %.x
	$(CC) $(CFLAGS) -I.. -xc -c $< -o $@

../libaw-fs.a:
	$(MAKE) -C..

.PHONY: clean
clean:
	rm -f test test.o

