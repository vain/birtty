CFLAGS += -Wall -Wextra -O3 `pkg-config --cflags ncurses`
LDLIBS += -lm `pkg-config --libs ncurses`

.PHONY: all clean
all: b

clean:
	rm -f b
