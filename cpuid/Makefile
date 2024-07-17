CC = clang-18

cpuid0: main.o cpuid0.o
	$(CC) -o $@ $^

main.o: main.c
	$(CC) -nostdlib -c -o $@ $^

cpuid0.o: cpuid0.S
	$(CC) -nostdlib -c -o $@ $^

clean:
	rm -f main.o
	rm -f cpuid0.o
	rm -f cpuid0

run: cpuid0
	./$<