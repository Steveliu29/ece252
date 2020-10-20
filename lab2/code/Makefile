all: paster
paster: crc.h crc.c catpng.c helper.c lab_png.h lab_png.c paster.c zutil.h zutil.c
	gcc -std=c99 paster.c -o paster -pthread -lz -lcurl

clean:
	rm -f paster