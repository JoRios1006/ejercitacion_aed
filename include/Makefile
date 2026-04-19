CC      = gcc
CFLAGS  = -nostdlib -fno-omit-frame-pointer -fno-builtin \
           -O2 -ffunction-sections -fdata-sections \
           -fno-unwind-tables -fno-asynchronous-unwind-tables \
           -fno-stack-protector -fno-ident \
           -Wl,--gc-sections \
           -Wl,--build-id=none \
           -Wl,-N -Wl,-T,tiny.ld

TARGET  = prog
SRC     = example.c

all: $(TARGET)
	sstrip $(TARGET)
	@wc -c $(TARGET)

$(TARGET): $(SRC) tiny.ld
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
