all: findpng2
findpng2: findpng2.c Queue.h Queue.c
	gcc -std=gnu99 findpng2.c -o findpng2 -lcurl -lxml2 -I/usr/include/libxml2 -pthread

clean:
	rm -f findpng2 *.txt