
#ifndef TGAFILE_H
#define TGAFILE_H

typedef struct tgafile_s
{
	// image data as RGB bytes
	unsigned char *data;
	// width of image
	unsigned int width;
	// height of image
	unsigned int height;
}
tgafile_t;

tgafile_t *loadtga(char *filename);
void freetga(tgafile_t *f);
int savetga_rgb24_topdown(char *filename, unsigned char *pixels, unsigned int width, unsigned int height);
int savetga_rgb32_topdown(char *filename, unsigned int *pixels, unsigned int width, unsigned int height);

#endif
