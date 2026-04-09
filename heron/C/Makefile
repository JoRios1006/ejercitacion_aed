TARGET = a.out
SRC = heron.c

CC = gcc
STRIP = strip

# Added -fno-pie and -no-pie to simplify the binary structure
CFLAGS = -Os -nostdlib -ffreestanding \
         -fno-stack-protector \
         -fno-unwind-tables \
         -fno-asynchronous-unwind-tables \
         -fno-ident \
         -fno-pie

LDFLAGS = -Wl,-n,--build-id=none -Wl,-z,max-page-size=0x1000 -no-pie -s

all: $(TARGET)
	@echo "Final binary size:"
	@ls -l $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)
	$(STRIP) -R .comment -R .note -R .note.gnu.property $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
