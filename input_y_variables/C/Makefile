CC      = gcc
CFLAGS  = -static -Wall -Wextra

SRC     = area.c
TARGET  = area

# --- con libc (para desarrollo/debug) ---
build:
	$(CC) $(CFLAGS) -O1 -o $(TARGET) $(SRC)

build-O3:
	$(CC) $(CFLAGS) -O3 -o $(TARGET)_O3 $(SRC)

# --- sin libc 
nostdlib:
	$(CC) $(CFLAGS) -O1 -nostdlib -o $(TARGET)_bare $(SRC)

nostdlib-O3:
	$(CC) $(CFLAGS) -O3 -nostdlib -o $(TARGET)_bare_O3 $(SRC)

# --- bench ---
bench: build 
	@echo "=== libc ==="

	perf stat -r 100 -e cycles,instructions,cache-misses \
sh -c 'echo -e "3.5\n2.0" | ./$(TARGET)'

bench_bare: build nostdlib 
	@echo "=== libc ===" 
	echo -e "3.5\n2.0" | perf stat -e ./$(TARGET)
	@echo "=== bare ===" 
	echo -e "3.5\n2.0" | perf -e stat ./$(TARGET)_bare

hyperfine: build 
	hyperfine \
		--input <(echo -e "3.5\n2.0") \
		'./$(TARGET)' './$(TARGET)_bare'

clean:
	rm -f $(TARGET) $(TARGET)_O3 $(TARGET)_bare $(TARGET)_bare_O3
