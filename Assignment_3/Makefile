.PHONY: all clean

all: disklist diskget diskput diskinfo

disklist: disklist.c diskutils.c
	gcc -Wall disklist.c diskutils.c -o disklist

diskget: diskget.c diskutils.c
	gcc -Wall diskget.c diskutils.c -o diskget

diskput: diskput.c diskutils.c
	gcc -Wall diskput.c diskutils.c -o diskput

diskinfo: diskinfo.c diskutils.c
	gcc -Wall diskinfo.c diskutils.c -o diskinfo

clean:
	-rm -rf disklist diskget diskput diskinfo