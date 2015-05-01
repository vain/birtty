CFLAGS += -Wall -Wextra -O3
LDLIBS += -lm

.PHONY: all clean
all: birtty

clean:
	rm -f birtty
