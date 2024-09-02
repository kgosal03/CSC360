# Makefile to automate the build and clean process
.PHONY: all clean

# Default target when no arguments passed
all: pman

# 'pman' has dependency on main.c and linked_list.c
# So it complies them into object files and links to executable 'pman'
pman: main.c linked_list.c
	gcc -Wall main.c linked_list.c -o pman

# 'clean' removes the 'pman' executable
clean:
	-rm -rf pman
