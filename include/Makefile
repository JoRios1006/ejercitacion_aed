CC      = gcc
CFLAGS  = -nostdlib -g -fno-omit-frame-pointer -fno-builtin \
           -O0 -ffunction-sections -fdata-sections \
           -fno-unwind-tables -fno-asynchronous-unwind-tables \
           -fno-stack-protector -fno-ident \
           -Wl,--gc-sections \
           -Wl,--build-id=none

TARGET  = prog
SRC     = example.c

all: $(TARGET)
	@wc -c $(TARGET)

$(TARGET): $(SRC) tiny.ld
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
