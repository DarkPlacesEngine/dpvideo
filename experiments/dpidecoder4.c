
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "tgafile.h"

#include "hz_read.c"

void Error (char *message, ...)
{
	va_list argptr;

	puts("ERROR: ");

	va_start (argptr,message);
	vprintf (message,argptr);
	va_end (argptr);
	putchar('\n');
#if _DEBUG && WIN32
	printf ("press a key\n");
	getchar();
#endif
	exit (1);
}

#if 0
// Haar
#define WAVESIZE 2
double wavelet [WAVESIZE] =
{
	0.707106781187,
	0.707106781187
};
#elif 0
// Daub4 wavelet
#define WAVESIZE 4
double wavelet [WAVESIZE] =
{
	0.4829629131445341,
	0.8365163037378077,
	0.2241438680420134,
	-0.1294095225512603,
};
#elif 0
// Daub6 wavelet
#define WAVESIZE 6
double wavelet [WAVESIZE] =
{
	0.3326705529500825,
	0.8068915093110924,
	0.4598775021184914,
	-0.1350110200102546,
	-0.0854412738820267,
	0.0352262918857095,
};
#elif 1
// Daub8 wavelet
#define WAVESIZE 8
double wavelet [WAVESIZE] =
{
	0.2303778133088964,
	0.7148465705529154,
	0.6308807679398587,
	-0.0279837694168599,
	-0.1870348117190931,
	0.0308413818355607,
	0.0328830116668852,
	-0.0105974017850690
};
#endif
#define WAVELETPREFETCH (WAVESIZE - 2)

#define WAVESTEPS 2
#define WAVEBUFFERS (WAVESTEPS + 1)

typedef struct
{
	int width, height;
	unsigned char *pixels;
}
dpiinfo_t;

// these are the waveforms generated from the wavelet function,
// [0] is the low pass waveform (averaging pixels - basically),
// [1], [2], and [3] are the high pass waveforms (storing intensities
// differenced against the lowpass).
// the high pass waveforms are made by reversing the order of numbers from
// the wavelow, and negating every even index (0, 2, 4, etc) on one or both
// axis.
// (note: if a different wavelet is used, other methods may be needed - if
//  the wavelet length is odd, negating should be on odd indices rather than
// even indices, and if it is an asymmetric wavelet (different analysis and
// synthesis) code changes would be necessary)
static float wave[WAVESIZE][WAVESIZE][4];

static void waveletsetup(void)
{
	int x, y;
	for (y = 0;y < WAVESIZE;y++)
	{
		for (x = 0;x < WAVESIZE;x++)
		{
			wave[y][x][0] = (wavelet[               x] * wavelet[               y]);
			wave[y][x][1] = (wavelet[WAVESIZE - 1 - x] * wavelet[               y]) * (((x    ) & 1) ? -1.0 : 1.0);
			wave[y][x][2] = (wavelet[               x] * wavelet[WAVESIZE - 1 - y]) * (((y    ) & 1) ? -1.0 : 1.0);
			wave[y][x][3] = (wavelet[WAVESIZE - 1 - x] * wavelet[WAVESIZE - 1 - y]) * (((x + y) & 1) ? -1.0 : 1.0);
		}
	}
}

#define HUFFMAN 1
#define HUFFMANREAD 1
static void dpi_decompressrle(hz_bitstream_readblocks_t *blocks, float *out, int length)
{
	int maxcount, vrange1, vrange2, vrange3, vmin1, vmin2, vmin3, rlebits, valuebits1, valuebits2, valuebits3, count, i;
	float valuescale1, valuescale2, valuescale3, o1, o2, o3;
#if HUFFMAN
	hzhuffmanreadtree_t *qrletree, *qvaluetree1, *qvaluetree2, *qvaluetree3;
#endif
	maxcount = hz_bitstream_read_bits(blocks, 8) + 1;
	vrange1 = hz_bitstream_read_bits(blocks, 16) + 1;
	vrange2 = hz_bitstream_read_bits(blocks, 16) + 1;
	vrange3 = hz_bitstream_read_bits(blocks, 16) + 1;
	vmin1 = -hz_bitstream_read_bits(blocks, 16);
	vmin2 = -hz_bitstream_read_bits(blocks, 16);
	vmin3 = -hz_bitstream_read_bits(blocks, 16);
	valuescale1 = hz_bitstream_read_bits(blocks, 16);
	valuescale2 = hz_bitstream_read_bits(blocks, 16);
	valuescale3 = hz_bitstream_read_bits(blocks, 16);
	for (rlebits = 0;maxcount > (1 << rlebits);rlebits++);
	for (valuebits1 = 0;vrange1 > (1 << valuebits1);valuebits1++);
	for (valuebits2 = 0;vrange2 > (1 << valuebits2);valuebits2++);
	for (valuebits3 = 0;vrange3 > (1 << valuebits3);valuebits3++);
	#if HUFFMAN
	qrletree = hz_huffman_read_newtree(maxcount);
	qvaluetree1 = hz_huffman_read_newtree(vrange1);
	qvaluetree2 = hz_huffman_read_newtree(vrange2);
	qvaluetree3 = hz_huffman_read_newtree(vrange3);
	if (hz_huffman_read_readtree(blocks, qrletree))
		Error("qrletree read error\n");
	if (hz_huffman_read_readtree(blocks, qvaluetree1))
		Error("qvaluetree read error\n");
	if (hz_huffman_read_readtree(blocks, qvaluetree2))
		Error("qvaluetree read error\n");
	if (hz_huffman_read_readtree(blocks, qvaluetree3))
		Error("qvaluetree read error\n");
	#endif
	for (i = 0;i < length * 3;)
	{
		#if HUFFMANREAD
		count = hz_huffman_read_readsymbol(blocks, qrletree) + 1;
		#else
		count = hz_bitstream_read_bits(blocks, rlebits) + 1;
		#endif
		if (count < 1 || count > maxcount)
			Error("invalid count (%i, maxcount %i)\n", count, maxcount);
		#if HUFFMANREAD
		o1 = hz_huffman_read_readsymbol(blocks, qvaluetree1);
		o2 = hz_huffman_read_readsymbol(blocks, qvaluetree2);
		o3 = hz_huffman_read_readsymbol(blocks, qvaluetree3);
		#else
		o1 = hz_bitstream_read_bits(blocks, valuebits1);
		o2 = hz_bitstream_read_bits(blocks, valuebits2);
		o3 = hz_bitstream_read_bits(blocks, valuebits3);
		#endif
		if (o1 < 0 || o1 >= vrange1)
			Error("invalid value\n");
		if (o2 < 0 || o2 >= vrange2)
			Error("invalid value\n");
		if (o3 < 0 || o3 >= vrange3)
			Error("invalid value\n");
		o1 = (o1 + vmin1) * valuescale1;
		o2 = (o2 + vmin2) * valuescale2;
		o3 = (o3 + vmin3) * valuescale3;
		if (count < 1)
			Error("RLE read error (low pass) invalid count %i\n", count);
		if ((i + count * 3) > length * 3)
			Error("RLE read error (low pass)\n");
		while (count--)
		{
			out[i++] = o1;
			out[i++] = o2;
			out[i++] = o3;
		}
	}
	#if HUFFMAN
	hz_huffman_read_freetree(qrletree);qrletree = NULL;
	hz_huffman_read_freetree(qvaluetree1);qvaluetree1 = NULL;
	hz_huffman_read_freetree(qvaluetree2);qvaluetree2 = NULL;
	hz_huffman_read_freetree(qvaluetree3);qvaluetree3 = NULL;
	#endif
}

static void dpi_converttorgb(float *in, unsigned char *out, int width, int height, int stride)
{
	int x, y, a;
	stride -= width * 3;
	for (y = 0;y < height;y++, out += stride)
	{
		for (x = 0;x < width;x++)
		{
			a = (int) (*in++ + 0.5);*out++ = (a >= 0 ? (a <= 255 ? a : 255) : 0);
			a = (int) (*in++ + 0.5);*out++ = (a >= 0 ? (a <= 255 ? a : 255) : 0);
			a = (int) (*in++ + 0.5);*out++ = (a >= 0 ? (a <= 255 ? a : 255) : 0);
		}
	}
}

static int dpi_decompressimage(dpiinfo_t *dpiinfo, hz_bitstream_readblocks_t *blocks)
{
	int passdatawidth[WAVEBUFFERS], passdataheight[WAVEBUFFERS], i, a, x, y, astart, wsize, width, height, steps, pos, iny;
	float *passdata[WAVEBUFFERS], o0r, o0g, o0b, o1r, o1g, o1b, o2r, o2g, o2b, o3r, o3g, o3b, *in, *out, *row, *pix, *w, w0, w1, w2, w3, *wrow;
	int failed;
	float *decompressedbuffer[3];

	passdatawidth[0] = dpiinfo->width;
	passdataheight[0] = dpiinfo->height;
	decompressedbuffer[0] = malloc(passdatawidth[0] * passdataheight[0] * 3 * sizeof(**decompressedbuffer));
	decompressedbuffer[1] = malloc(passdatawidth[0] * passdataheight[0] * 3 * sizeof(**decompressedbuffer));
	decompressedbuffer[2] = malloc(passdatawidth[0] * passdataheight[0] * 3 * sizeof(**decompressedbuffer));
	passdata[0] = malloc(passdatawidth[0] * passdataheight[0] * 3 * sizeof(**passdata)/* + 4*/);
	//passdata[0][passdatawidth[0] * passdataheight[0]] = -999999;
	failed = passdata[0] == NULL;
	for (i = 1;i < WAVEBUFFERS;i++)
	{
		passdatawidth[i] = ((passdatawidth[i - 1] + WAVELETPREFETCH + 1) >> 1);
		passdataheight[i] = ((passdataheight[i - 1] + WAVELETPREFETCH + 1) >> 1);
		passdata[i] = malloc(passdatawidth[i] * passdataheight[i] * 3 * sizeof(**passdata)/* + 4*/);
		if (passdata[i] == NULL)
			failed = 1;
		//passdata[i][passdatawidth[i] * passdataheight[i]] = -999999;
	}

	waveletsetup();
	if (failed)
		Error("blah\n");
	if (!failed)
	{
		dpi_decompressrle(blocks, passdata[WAVESTEPS], passdatawidth[WAVESTEPS] * passdataheight[WAVESTEPS]);
		for (steps = WAVESTEPS;steps;steps--)
		{
			dpi_decompressrle(blocks, decompressedbuffer[0], passdatawidth[steps] * passdataheight[steps]);
			dpi_decompressrle(blocks, decompressedbuffer[1], passdatawidth[steps] * passdataheight[steps]);
			dpi_decompressrle(blocks, decompressedbuffer[2], passdatawidth[steps] * passdataheight[steps]);
			pos = 0;

			width = passdatawidth[steps - 1];
			height = passdataheight[steps - 1];
			out = passdata[steps - 1];
			memset(out, 0, width * height * 3 * sizeof(*out));

			// only the lowpass filter is fetched from the previous stage
			in = passdata[steps];
			for (y = 0;y < height;y++)
			{
				row = out + y * width * 3;
				memset(row, 0, width * 3 * sizeof(*out));
				for (iny = y & ~1;(y - iny) < WAVESIZE;iny -= 2)
				{
					pos = ((iny + WAVELETPREFETCH) >> 1) * passdatawidth[steps] * 3;
					wrow = &wave[(y - iny)][0][0];
					for (x = -WAVELETPREFETCH;x < width;x += 2)
					{
						o0r = in[pos + 0];o0g = in[pos + 1];o0b = in[pos + 2];
						o1r = decompressedbuffer[0][pos + 0];o1g = decompressedbuffer[0][pos + 1];o1b = decompressedbuffer[0][pos + 2];
						o2r = decompressedbuffer[1][pos + 0];o2g = decompressedbuffer[1][pos + 1];o2b = decompressedbuffer[1][pos + 2];
						o3r = decompressedbuffer[2][pos + 0];o3g = decompressedbuffer[2][pos + 1];o3b = decompressedbuffer[2][pos + 2];
						pos += 3;
						wsize = width - x;
						if (wsize > WAVESIZE)
							wsize = WAVESIZE;
						if (x < 0)
							astart = -x;
						else
							astart = 0;
						pix = row + (x + astart) * 3;
						w = wrow + astart * WAVESIZE;
						if (o1r || o1g || o1b)
						{
							if (o2r || o2g || o2b)
							{
								if (o3r || o3g || o3b)
								{
									for (a = astart, i = 0;a < wsize;a++)
									{
										w0 = w[0];
										w1 = w[1];
										w2 = w[2];
										w3 = w[3];
										w += 4;
										pix[i++] += o0r * w0 + o1r * w1 + o2r * w2 + o3r * w3;
										pix[i++] += o0g * w0 + o1g * w1 + o2g * w2 + o3g * w3;
										pix[i++] += o0b * w0 + o1b * w1 + o2b * w2 + o3b * w3;
									}
								}
								else
								{
									for (a = astart, i = 0;a < wsize;a++)
									{
										w0 = w[0];
										w1 = w[1];
										w2 = w[2];
										w += 4;
										pix[i++] += o0r * w0 + o1r * w1 + o2r * w2;
										pix[i++] += o0g * w0 + o1g * w1 + o2g * w2;
										pix[i++] += o0b * w0 + o1b * w1 + o2b * w2;
									}
								}
							}
							else
							{
								if (o3r || o3g || o3b)
								{
									for (a = astart, i = 0;a < wsize;a++)
									{
										w0 = w[0];
										w1 = w[1];
										w3 = w[3];
										w += 4;
										pix[i++] += o0r * w0 + o1r * w1 + o3r * w3;
										pix[i++] += o0g * w0 + o1g * w1 + o3g * w3;
										pix[i++] += o0b * w0 + o1b * w1 + o3b * w3;
									}
								}
								else
								{
									for (a = astart, i = 0;a < wsize;a++)
									{
										w0 = w[0];
										w1 = w[1];
										w += 4;
										pix[i++] += o0r * w0 + o1r * w1;
										pix[i++] += o0g * w0 + o1g * w1;
										pix[i++] += o0b * w0 + o1b * w1;
									}
								}
							}
						}
						else
						{
							if (o2r || o2g || o2b)
							{
								if (o3r || o3g || o3b)
								{
									for (a = astart, i = 0;a < wsize;a++)
									{
										w0 = w[0];
										w2 = w[2];
										w3 = w[3];
										w += 4;
										pix[i++] += o0r * w0 + o2r * w2 + o3r * w3;
										pix[i++] += o0g * w0 + o2g * w2 + o3g * w3;
										pix[i++] += o0b * w0 + o2b * w2 + o3b * w3;
									}
								}
								else
								{
									for (a = astart, i = 0;a < wsize;a++)
									{
										w0 = w[0];
										w2 = w[2];
										w += 4;
										pix[i++] += o0r * w0 + o2r * w2;
										pix[i++] += o0g * w0 + o2g * w2;
										pix[i++] += o0b * w0 + o2b * w2;
									}
								}
							}
							else
							{
								if (o3r || o3g || o3b)
								{
									for (a = astart, i = 0;a < wsize;a++)
									{
										w0 = w[0];
										w3 = w[3];
										w += 4;
										pix[i++] += o0r * w0 + o3r * w3;
										pix[i++] += o0g * w0 + o3g * w3;
										pix[i++] += o0b * w0 + o3b * w3;
									}
								}
								else
								{
									for (a = astart, i = 0;a < wsize;a++)
									{
										w0 = w[0];
										w += 4;
										pix[i++] += o0r * w0;
										pix[i++] += o0g * w0;
										pix[i++] += o0b * w0;
									}
								}
							}
						}
					}
				}
			}
		}
		dpi_converttorgb(passdata[0], dpiinfo->pixels, dpiinfo->width, dpiinfo->height, dpiinfo->width * 3);
	}

	for (i = 0;i < 3;i++)
		if (decompressedbuffer[i])
			free(decompressedbuffer[i]);
	for (i = 0;i < WAVEBUFFERS;i++)
		if (passdata[i])
			free(passdata[i]);

	return failed;
}

static dpiinfo_t *dpi_readimagergb(char *filename)
{
	int datalength;
	char idstring[8];
	hz_bitstream_read_t *bitstream;
	hz_bitstream_readblocks_t *blocks;
	dpiinfo_t *dpiinfo;
	bitstream = hz_bitstream_read_open(filename);
	if (bitstream)
	{
		blocks = hz_bitstream_read_blocks_new();
		if (blocks)
		{
			hz_bitstream_read_blocks_read(blocks, bitstream, 17);
			hz_bitstream_read_bytes(blocks, idstring, 8);
			if (!memcmp(idstring, "DPIMAGE", 8))
			{
				// DPIMAGE
				if (hz_bitstream_read_byte(blocks) == 1)
				{
					// type 1
					dpiinfo = malloc(sizeof(dpiinfo_t));
					if (dpiinfo)
					{
						dpiinfo->width = hz_bitstream_read_short(blocks);
						dpiinfo->height = hz_bitstream_read_short(blocks);
						datalength = hz_bitstream_read_int(blocks);
						hz_bitstream_read_blocks_free(blocks);
						blocks = hz_bitstream_read_blocks_new();
						hz_bitstream_read_blocks_read(blocks, bitstream, datalength);
						dpiinfo->pixels = malloc(dpiinfo->width * dpiinfo->height * 3);
						if (dpiinfo->pixels)
						{
							if (!dpi_decompressimage(dpiinfo, blocks))
								return dpiinfo;
							free(dpiinfo->pixels);
						}
						free(dpiinfo);
					}
				}
			}
			hz_bitstream_read_blocks_free(blocks);
		}
		hz_bitstream_read_close(bitstream);
	}
	return NULL;
}

void dpi_freeimage(dpiinfo_t *dpiinfo)
{
	if (dpiinfo == NULL)
		return;
	if (dpiinfo->pixels)
		free(dpiinfo->pixels);
	free(dpiinfo);
}

void usage(void)
{
	printf("usage: dpvdecoder <inputname.dpi> <outputname.tga>\n");
}

int main(int argc, char **argv)
{
	dpiinfo_t *dpiinfo;
	if (argc != 3)
	{
		usage();
		return 1;
	}
	if ((dpiinfo = dpi_readimagergb(argv[1])))
	{
		savetga_rgb24_topdown(argv[2], dpiinfo->pixels, dpiinfo->width, dpiinfo->height);
		dpi_freeimage(dpiinfo);
	}
	else
		printf("error decompressing \"%s\", may not be a DPIMAGE file\n", argv[1]);
	return 0;
}
