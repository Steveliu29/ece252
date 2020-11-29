all: findpng3
findpng3: findpng3.c Queue.h Queue.c
	gcc -std=gnu99 findpng3.c -o findpng3 -lcurl -lxml2 -I/usr/include/libxml2

clean:
	rm -f findpng3 *.txt
