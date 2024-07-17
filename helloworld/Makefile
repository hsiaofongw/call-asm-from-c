CC = clang-18

helloworld: main.o hello.o
	$(CC) -o $@ $^

main.o: main.c
	$(CC) -nostdlib -c -o $@ $^

hello.o: hello.S
	$(CC) -nostdlib -c -o $@ $^

clean:
	rm -f main.o
	rm -f hello.o
	rm -r helloworld

run: helloworld
	./$<
