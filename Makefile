CFLAGS += -Wall -Wextra -O3
LDLIBS += -lm

.PHONY: all clean
all: b

clean:
	rm -f b
