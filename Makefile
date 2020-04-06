TARGET:=tc

CC:=gcc
CFLAGS:=-Iinclude -Wall -Wextra -g

SOURCES:=$(wildcard src/*.c)
OBJECTS:=$(patsubst src/%.c,build/%.o,$(SOURCES))

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

build/%.o: src/%.c
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) -MD -o $@ -c $<


clean:
	rm -rf build/ $(TARGET)

-include $(OBJECTS:.o=.d)
