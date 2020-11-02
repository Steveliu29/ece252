all: paster2
paster2: crc.h crc.c helper.c lab_png.h lab_png.c paster2.c zutil.h zutil.c
	gcc -std=c99 paster2.c -o paster2 -lz -lcurl -pthread

clean:
	rm -f paster2