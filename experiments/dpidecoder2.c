
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
static double wave[WAVESIZE][WAVESIZE][4];

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
	int maxcount, vrange, vmin, rlebits, valuebits, count, i;
	float valuescale, o;
#if HUFFMAN
	hzhuffmanreadtree_t *qrletree, *qvaluetree;
#endif
	maxcount = hz_bitstream_read_bits(blocks, 8) + 1;
	vrange = hz_bitstream_read_bits(blocks, 16) + 1;
	vmin = -hz_bitstream_read_bits(blocks, 16);
	valuescale = hz_bitstream_read_bits(blocks, 16);
	for (rlebits = 0;maxcount > (1 << rlebits);rlebits++);
	for (valuebits = 0;vrange > (1 << valuebits);valuebits++);
	#if HUFFMAN
	qrletree = hz_huffman_read_newtree(maxcount);
	qvaluetree = hz_huffman_read_newtree(vrange);
	if (hz_huffman_read_readtree(blocks, qrletree))
		Error("qrletree read error\n");
	if (hz_huffman_read_readtree(blocks, qvaluetree))
		Error("qvaluetree read error\n");
	#endif
	for (i = 0;i < length;)
	{
		#if HUFFMANREAD
		count = hz_huffman_read_readsymbol(blocks, qrletree) + 1;
		#else
		count = hz_bitstream_read_bits(blocks, rlebits) + 1;
		#endif
		if (count < 1 || count > maxcount)
			Error("invalid count (%i, maxcount %i) on line %i\n", count, maxcount, __LINE__);
		#if HUFFMANREAD
		o = hz_huffman_read_readsymbol(blocks, qvaluetree);
		#else
		o = hz_bitstream_read_bits(blocks, valuebits);
		#endif
		if (o < 0 || o >= vrange)
			Error("invalid value\n");
		o = (o + vmin) * valuescale;
		if (count < 1)
			Error("RLE read error (low pass) invalid count %i\n", count);
		if ((i + count) > length)
			Error("RLE read error (low pass)\n");
		while (count--)
			out[i++] = o;
	}
	#if HUFFMAN
	hz_huffman_read_freetree(qrletree);qrletree = NULL;
	hz_huffman_read_freetree(qvaluetree);qvaluetree = NULL;
	#endif
}

static int dpi_decompressimage(dpiinfo_t *dpiinfo, hz_bitstream_readblocks_t *blocks)
{
	int passdatawidth[WAVEBUFFERS], passdataheight[WAVEBUFFERS], i, a, b, x, y, astart, bstart, wsize, hsize, colorindex, w, h, steps, pos;
	float *passdata[WAVEBUFFERS], o0, o1, o2, o3, *in, *out, *row, *pix;
	int failed;
	float *decompressedbuffer[3];

	passdatawidth[0] = dpiinfo->width;
	passdataheight[0] = dpiinfo->height;
	decompressedbuffer[0] = malloc(passdatawidth[0] * passdataheight[0] * sizeof(**decompressedbuffer));
	decompressedbuffer[1] = malloc(passdatawidth[0] * passdataheight[0] * sizeof(**decompressedbuffer));
	decompressedbuffer[2] = malloc(passdatawidth[0] * passdataheight[0] * sizeof(**decompressedbuffer));
	passdata[0] = malloc(passdatawidth[0] * passdataheight[0] * sizeof(**passdata)/* + 4*/);
	//passdata[0][passdatawidth[0] * passdataheight[0]] = -999999;
	failed = passdata[0] == NULL;
	for (i = 1;i < WAVEBUFFERS;i++)
	{
		passdatawidth[i] = ((passdatawidth[i - 1] + WAVELETPREFETCH + 1) >> 1);
		passdataheight[i] = ((passdataheight[i - 1] + WAVELETPREFETCH + 1) >> 1);
		passdata[i] = malloc(passdatawidth[i] * passdataheight[i] * sizeof(**passdata)/* + 4*/);
		if (passdata[i] == NULL)
			failed = 1;
		//passdata[i][passdatawidth[i] * passdataheight[i]] = -999999;
	}

	waveletsetup();
	if (failed)
		Error("blah\n");
	if (!failed)
	{
		for (colorindex = 0;colorindex < 3;colorindex++)
		{
			w = passdatawidth[WAVESTEPS];
			h = passdataheight[WAVESTEPS];
			dpi_decompressrle(blocks, passdata[WAVESTEPS], w * h);
			for (steps = WAVESTEPS;steps;steps--)
			{
				dpi_decompressrle(blocks, decompressedbuffer[0], passdatawidth[steps] * passdataheight[steps]);
				dpi_decompressrle(blocks, decompressedbuffer[1], passdatawidth[steps] * passdataheight[steps]);
				dpi_decompressrle(blocks, decompressedbuffer[2], passdatawidth[steps] * passdataheight[steps]);
				pos = 0;

				w = passdatawidth[steps - 1];
				h = passdataheight[steps - 1];
				out = passdata[steps - 1];
				memset(out, 0, w * h * sizeof(*out));

				// only the lowpass filter is fetched from the previous stage
				in = passdata[steps];
				for (y = -WAVELETPREFETCH;y < h;y += 2)
				{
					hsize = h - y;
					if (hsize > WAVESIZE)
						hsize = WAVESIZE;
					if (y < 0)
						bstart = -y;
					else
						bstart = 0;
					row = out + (y + bstart) * w;
					for (x = -WAVELETPREFETCH;x < w;x += 2)
					{
						// high pass filters are RLE decompressed from the stream on-demand
						o0 = in[pos];
						o1 = decompressedbuffer[0][pos];
						o2 = decompressedbuffer[1][pos];
						o3 = decompressedbuffer[2][pos++];
						wsize = w - x;
						if (wsize > WAVESIZE)
							wsize = WAVESIZE;
						if (x < 0)
							astart = -x;
						else
							astart = 0;
						if (o1)
						{
							if (o2)
							{
								if (o3)
								{
									for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
										for (a = astart;a < wsize;a++)
											pix[a] += o0 * wave[b][a][0] + o1 * wave[b][a][1] + o2 * wave[b][a][2] + o3 * wave[b][a][3];
								}
								else
								{
									for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
										for (a = astart;a < wsize;a++)
											pix[a] += o0 * wave[b][a][0] + o1 * wave[b][a][1] + o2 * wave[b][a][2];
								}
							}
							else
							{
								if (o3)
								{
									for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
										for (a = astart;a < wsize;a++)
											pix[a] += o0 * wave[b][a][0] + o1 * wave[b][a][1] + o3 * wave[b][a][3];
								}
								else
								{
									for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
										for (a = astart;a < wsize;a++)
											pix[a] += o0 * wave[b][a][0] + o1 * wave[b][a][1];
								}
							}
						}
						else
						{
							if (o2)
							{
								if (o3)
								{
									for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
										for (a = astart;a < wsize;a++)
											pix[a] += o0 * wave[b][a][0] + o2 * wave[b][a][2] + o3 * wave[b][a][3];
								}
								else
								{
									for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
										for (a = astart;a < wsize;a++)
											pix[a] += o0 * wave[b][a][0] + o2 * wave[b][a][2];
								}
							}
							else
							{
								if (o3)
								{
									for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
										for (a = astart;a < wsize;a++)
											pix[a] += o0 * wave[b][a][0] + o3 * wave[b][a][3];
								}
								else
								{
									for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
										for (a = astart;a < wsize;a++)
											pix[a] += o0 * wave[b][a][0];
								}
							}
						}
					}
				}
			}
			for (i = 0;i < dpiinfo->width * dpiinfo->height;i++)
			{
				a = (int) (passdata[0][i] + 0.5);
				if (a < 0)
					a = 0;
				if (a > 255)
					a = 255;
				dpiinfo->pixels[i * 3 + colorindex] = a;
			}
		}
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
