test_parse: test_parse.c ../alloc.c ../err.c ../blob.c ../hash.c ../ringbuf.c ../pkt.c
	clang-18 -O0 -g3 -o $@ $^

test_queue: test_queue.c ../queue.c ../ringbuf.c
	clang-18 -O0 -g3 -o $@ $^

test_pq: test_pq.c ../priority_queue.c
	clang-18 -O0 -g3 -o $@ $^

clean:
	rm -f file1.bin
	rm -f file2.bin
	rm -f test_parse
	rm -f test_queue
	rm -f test_pq
