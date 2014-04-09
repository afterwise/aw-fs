
export CFLAGS += -Wall -Wextra

test: test.o ../libaw-fs.a
	clang $(LDFLAGS) -o $@ $^

%.o: %.c
	clang $(CFLAGS) -I.. -c $< -o $@

../libaw-fs.a:
	$(MAKE) -C..

.PHONY: clean
clean:
	rm -f test test.o

