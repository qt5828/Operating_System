TARGET	= sched
CFLAGS	= -g -c -D_POSIX_C_SOURCE -Iinclude
CFLAGS += -std=c99 -Wimplicit-function-declaration -Werror
CFLAGS += # Add your own cflags here if necessary
LDFLAGS	=

all: sched

sched: pa2.o parser.o sched.o
	gcc $(LDFLAGS) $^ -o $@

%.o: %.c
	gcc $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -rf $(TARGET) *.o *.dSYM
