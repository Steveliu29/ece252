all: pnginfo findpng catpng
pnginfo: pnginfo.c crc.h crc.c zutil.h zutil.c lab_png.h lab_png.c
	gcc -std=c99 pnginfo.c -o pnginfo -lz
findpng: findpng.c lab_png.h lab_png.c
	gcc -std=c99 findpng.c -o findpng -lz
catpng: catpng.c crc.h crc.c zutil.h zutil.c lab_png.h lab_png.c
	gcc -std=c99 catpng.c -o catpng -lz

clean:
	rm -f pnginfo findpng catpng