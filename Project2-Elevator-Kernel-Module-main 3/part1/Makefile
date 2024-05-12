all:
	gcc -o empty empty.c
	strace -o empty.trace ./empty
	gcc -o part1 part1.c
	strace -o part1.trace ./part1

clean:
	rm -f empty part1 empty.trace part1.trace
