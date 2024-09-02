# Makefile to automate the build and clean process
.PHONY: all clean

# Default target when no arguments passed
all: ACS

# 'ACS' has dependency on 'ACS.o' and 'queue.o'
# So it compiles them into object files and links to pthread library
ACS: ACS.o queue.o
	gcc -Wall -o ACS ACS.o queue.o -lpthread

# Compile 'ACS.c' into 'ACS.o'
ACS.o: ACS.c queue.h
	gcc -Wall -c ACS.c

# Compile 'queue.c' into 'queue.o'
queue.o: queue.c queue.h
	gcc -Wall -c queue.c

# 'clean' removes the 'ACS' executable and object files
clean:
	-rm -rf *.o ACS
