
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "file.h"
#include "tgafile.h"

/*
=============
LoadTGA
=============
*/
tgafile_t *loadtga(char *filename)
{
	tgafile_t *tgafile;
	int columns, rows, row, column;
	unsigned int datasize;
	unsigned char *pixbuf, *pixels, *fin, *data, *enddata;
	void *voiddata;
	// the contents of the targa header
	struct
	{
		unsigned char id_length, colormap_type, image_type;
		unsigned short colormap_index, colormap_length;
		unsigned char colormap_size;
		unsigned short x_origin, y_origin, width, height;
		unsigned char pixel_size, attributes;
	}
	targa;

	if (readfile(filename, &voiddata, &datasize) || datasize < 18)
		return NULL;
	data = (unsigned char *)voiddata;

	targa.id_length = data[0];
	targa.colormap_type = data[1];
	targa.image_type = data[2];

	targa.colormap_index = data[3] + data[4] * 256;
	targa.colormap_length = data[5] + data[6] * 256;
	targa.colormap_size = data[7];
	targa.x_origin = data[8] + data[9] * 256;
	targa.y_origin = data[10] + data[11] * 256;
	targa.width = data[12] + data[13] * 256;
	targa.height = data[14] + data[15] * 256;
	targa.pixel_size = data[16];
	targa.attributes = data[17];

	if (targa.image_type != 2 && targa.image_type != 10)
	{
		printf ("LoadTGA: Only type 2 and 10 targa RGB images supported\n");
		free(data);
		return NULL;
	}

	if (targa.colormap_type != 0 || (targa.pixel_size != 32 && targa.pixel_size != 24))
	{
		printf ("LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
		free(data);
		return NULL;
	}

	enddata = data + datasize;

	columns = targa.width;
	rows = targa.height;

	pixels = malloc(columns * rows * 3);
	if (!pixels)
	{
		printf ("LoadTGA: not enough memory for %i by %i image\n", columns, rows);
		free(data);
		return NULL;
	}

	fin = data + 18;
	if (targa.id_length != 0)
		fin += targa.id_length;  // skip TARGA image comment

	if (targa.image_type == 2)
	{
		// Uncompressed, RGB images
		for(row = rows - 1;row >= 0;row--)
		{
			pixbuf = pixels + row*columns*3;
			for(column = 0;column < columns;column++)
			{
				switch (targa.pixel_size)
				{
				case 24:
					if (fin + 3 > enddata)
						break;
					*pixbuf++ = fin[2];
					*pixbuf++ = fin[1];
					*pixbuf++ = fin[0];
					fin += 3;
					break;
				case 32:
					if (fin + 4 > enddata)
						break;
					*pixbuf++ = fin[2];
					*pixbuf++ = fin[1];
					*pixbuf++ = fin[0];
					// throw away alpha
					//*pixbuf++ = fin[3];
					fin += 4;
					break;
				}
			}
		}
	}
	else if (targa.image_type==10)
	{
		// Runlength encoded RGB images
		unsigned char red = 0, green = 0, blue = 0, alphabyte = 0, packetHeader, packetSize, j;
		for(row = rows - 1;row >= 0;row--)
		{
			pixbuf = pixels + row * columns * 3;
			for(column = 0;column < columns;)
			{
				if (fin >= enddata)
					goto outofdata;
				packetHeader = *fin++;
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80)
				{
					// run-length packet
					switch (targa.pixel_size)
					{
					case 24:
						if (fin + 3 > enddata)
							goto outofdata;
						blue = *fin++;
						green = *fin++;
						red = *fin++;
						alphabyte = 255;
						break;
					case 32:
						if (fin + 4 > enddata)
							goto outofdata;
						blue = *fin++;
						green = *fin++;
						red = *fin++;
						alphabyte = *fin++;
						break;
					}

					for(j = 0;j < packetSize;j++)
					{
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						// throw away alpha
						//*pixbuf++ = alphabyte;
						column++;
						if (column == columns)
						{
							// run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = pixels + row * columns * 3;
						}
					}
				}
				else
				{
					// non run-length packet
					for(j = 0;j < packetSize;j++)
					{
						switch (targa.pixel_size)
						{
						case 24:
							if (fin + 3 > enddata)
								goto outofdata;
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							fin += 3;
							break;
						case 32:
							if (fin + 3 > enddata)
								goto outofdata;
							*pixbuf++ = fin[2];
							*pixbuf++ = fin[1];
							*pixbuf++ = fin[0];
							// throw away alpha
							//*pixbuf++ = fin[3];
							fin += 4;
							break;
						}
						column++;
						if (column == columns)
						{
							// pixel packet run spans across rows
							column = 0;
							if (row > 0)
								row--;
							else
								goto breakOut;
							pixbuf = pixels + row * columns * 3;
						}
					}
				}
			}
			breakOut:;
		}
	}
outofdata:;

	free(data);
	tgafile = malloc(sizeof(*tgafile));
	memset(tgafile, 0, sizeof(*tgafile));
	tgafile->data = pixels;
	tgafile->width = columns;
	tgafile->height = rows;
	return tgafile;
}

void freetga(tgafile_t *f)
{
	if (f == NULL)
		return;
	free(f->data);
	free(f);
}

int savetga_rgb24_topdown(char *filename, unsigned char *pixels, unsigned int width, unsigned int height)
{
	unsigned int x, y;
	unsigned char *buffer, *buf;
	unsigned char *row;
	buffer = malloc(width * height * 3 + 18);
	if (buffer == NULL)
		return 1;
	memset(buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = (width >> 0) & 0xFF;
	buffer[13] = (width >> 8) & 0xFF;
	buffer[14] = (height >> 0) & 0xFF;
	buffer[15] = (height >> 8) & 0xFF;
	buffer[16] = 24;	// pixel size
	buf = buffer + 18;
	for (y = 0;y < height;y++)
	{
		row = pixels + (height - 1 - y) * width * 3;
		for (x = 0;x < width * 3;x += 3)
		{
			// write out as BGR
			*buf++ = row[x + 2] & 0xFF;
			*buf++ = row[x + 1] & 0xFF;
			*buf++ = row[x    ] & 0xFF;
		}
	}
	buf = buffer + 18;
	writefile(filename, buffer, width * height * 3 + 18);
	free(buffer);
	return 0;
}

int savetga_rgb32_topdown(char *filename, unsigned int *pixels, unsigned int width, unsigned int height)
{
	unsigned int x, y;
	unsigned char *buffer, *buf;
	unsigned int *row;
	buffer = malloc(width * height * 3 + 18);
	if (buffer == NULL)
		return 1;
	memset(buffer, 0, 18);
	buffer[2] = 2;		// uncompressed type
	buffer[12] = (width >> 0) & 0xFF;
	buffer[13] = (width >> 8) & 0xFF;
	buffer[14] = (height >> 0) & 0xFF;
	buffer[15] = (height >> 8) & 0xFF;
	buffer[16] = 24;	// pixel size
	buf = buffer + 18;
	for (y = 0;y < height;y++)
	{
		row = pixels + (height - 1 - y) * width;
		for (x = 0;x < width;x++)
		{
			// write out as BGR
			*buf++ = (row[x] >>  0) & 0xFF;
			*buf++ = (row[x] >>  8) & 0xFF;
			*buf++ = (row[x] >> 16) & 0xFF;
		}
	}
	buf = buffer + 18;
	writefile(filename, buffer, width * height * 3 + 18);
	free(buffer);
	return 0;
}
