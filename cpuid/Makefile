CC = clang-18

cpuid: main.o cpuid.o
	$(CC) -o $@ $^

main.o: main.c
	$(CC) -nostdlib -c -o $@ $^

cpuid.o: cpuid.S
	$(CC) -nostdlib -c -o $@ $^

clean:
	rm -f main.o
	rm -f cpuid.o
	rm -f cpuid

run: cpuid
	./$<