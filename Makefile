
COMPILER = gcc
FILESYSTEM_FILES = fs.c

build: $(FILESYSTEM_FILES)
	$(COMPILER) -g -DDEBUG $(FILESYSTEM_FILES) -o ssfs `pkg-config fuse --cflags --libs`
	echo 'To Mount: ./ssfs -f [mount point]'

clean:
	rm ssfs
