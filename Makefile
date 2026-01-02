CC := gcc

all: my_malloc.out

my_malloc.out: my_malloc.c
	$(CC) my_malloc.c -o my_malloc.out

clean:
	rm my_malloc.out
