
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "tgafile.h"

#include "hz_write.c"

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
static double wave[4][WAVESIZE][WAVESIZE];

static void waveletsetup(void)
{
	int x, y;
	for (y = 0;y < WAVESIZE;y++)
	{
		for (x = 0;x < WAVESIZE;x++)
		{
			wave[0][y][x] = (wavelet[               x] * wavelet[               y]);
			wave[1][y][x] = (wavelet[WAVESIZE - 1 - x] * wavelet[               y]) * (((x    ) & 1) ? -1.0 : 1.0);
			wave[2][y][x] = (wavelet[               x] * wavelet[WAVESIZE - 1 - y]) * (((y    ) & 1) ? -1.0 : 1.0);
			wave[3][y][x] = (wavelet[WAVESIZE - 1 - x] * wavelet[WAVESIZE - 1 - y]) * (((x + y) & 1) ? -1.0 : 1.0);
		}
	}
}

#define WAVESTEPS 2
#define WAVEBUFFERS (WAVESTEPS + 1)
#define HUFFMAN 1
#define HUFFMANWRITE 1

//static char *filterchunknames[4] = {"PASS----", "PASS--X-", "PASS---Y", "PASS--XY"};
static void dpi_writequantized(hz_bitstream_writeblocks_t *blocks, float *data, unsigned int length, unsigned int quantize, short *compressbuffer, int filternum)
{
	int i, a, n, count, compressbufferlength, vmin, vmax, vrange, maxcount, rlebits, valuebits;
	float o, q, iq;
#if HUFFMAN
	hzhuffmanwritetree_t *qrletree, *qvaluetree;
#endif
//#define PARANOIDBITS hz_bitstream_write_bits(blocks, 0xFEFEFEFE, 32);
#define PARANOIDBITS
	n = 2000000000;
	count = 0;
	compressbufferlength = 0;
	if (quantize > 256)
		quantize = 256;
	if (quantize < 1)
		quantize = 1;
	q = quantize * (1.0 / 256.0);
	iq = 1.0 / q;
	vmin = 0;
	vmax = 0;
	maxcount = 1;
	for (i = 0;i < length;i++)
	{
		o = data[i] * q;
		/*
		// float rounds toward zero
		if (o < 0)
			a = (int)(o - 0.5);
		else
			a = (int)(o + 0.5);
		*/
		a = (int)o;
		if (vmin > a)
			vmin = a;
		if (vmax < a)
			vmax = a;
#if 1
		if (n != a || count >= 256)
		{
			if (count)
			{
				if (maxcount < count)
					maxcount = count;
				compressbuffer[compressbufferlength++] = count - 1;
				compressbuffer[compressbufferlength++] = n;
			}
			n = a;
			count = 0;
		}
		count++;
#else
		compressbuffer[compressbufferlength++] = 0;
		compressbuffer[compressbufferlength++] = a;
#endif
	}
	if (count)
	{
		if (maxcount < count)
			maxcount = count;
		compressbuffer[compressbufferlength++] = count - 1;
		compressbuffer[compressbufferlength++] = n;
	}

	vrange = vmax - vmin + 1;
	for (rlebits = 0;maxcount > (1 << rlebits);rlebits++);
	for (valuebits = 0;vrange > (1 << valuebits);valuebits++);
	//rlebits = 0;
	//valuebits = 16;
	//a = hz_bitstream_write_sizeofblocks(blocks);
	//hz_bitstream_write_bytes(blocks, filterchunknames[filternum], 8);
	PARANOIDBITS
	hz_bitstream_write_bits(blocks, maxcount - 1, 8);
	hz_bitstream_write_bits(blocks, vrange - 1, 16);
	hz_bitstream_write_bits(blocks, -vmin, 16);
	hz_bitstream_write_bits(blocks, (int) iq, 16);
#if HUFFMAN
	qrletree = hz_huffman_write_newtree(maxcount);
	qvaluetree = hz_huffman_write_newtree(vrange);
	hz_huffman_write_clearcounts(qrletree);
	hz_huffman_write_clearcounts(qvaluetree);
	//for (i = 0;i < maxcount;i++)
	//	hz_huffman_write_countsymbol(qrletree, i);
	//for (i = 0;i < vrange;i++)
	//	hz_huffman_write_countsymbol(qvaluetree, i);
	for (i = 0;i < compressbufferlength;i += 2)
	{
		hz_huffman_write_countsymbol(qrletree, compressbuffer[i + 0]);
		hz_huffman_write_countsymbol(qvaluetree, compressbuffer[i + 1] - vmin);
	}
	hz_huffman_write_buildtree(qrletree);
	hz_huffman_write_buildtree(qvaluetree);
	PARANOIDBITS
	hz_huffman_write_writetree(blocks, qrletree);
	//printf("pass %s, byte length %i\n", filterchunknames[filternum], hz_bitstream_write_sizeofblocks(blocks) - a);
	PARANOIDBITS
	hz_huffman_write_writetree(blocks, qvaluetree);
	//printf("pass %s, byte length %i\n", filterchunknames[filternum], hz_bitstream_write_sizeofblocks(blocks) - a);
#endif
	PARANOIDBITS
	for (i = 0;i < compressbufferlength;i += 2)
	{
		if (compressbuffer[i + 1] < vmin || compressbuffer[i + 1] > vmax)
			printf("invalid value %i (outside range %i to %i)\n", compressbuffer[i + 1], vmin, vmax);
#if HUFFMANWRITE
		hz_huffman_write_writesymbol(blocks, qrletree, compressbuffer[i + 0]);
		PARANOIDBITS
		hz_huffman_write_writesymbol(blocks, qvaluetree, compressbuffer[i + 1] - vmin);
		PARANOIDBITS
#else
		hz_bitstream_write_bits(blocks, compressbuffer[i + 0], rlebits);
		PARANOIDBITS
		hz_bitstream_write_bits(blocks, compressbuffer[i + 1] - vmin, valuebits);
		PARANOIDBITS
#endif
	}
#if HUFFMAN
	hz_huffman_write_freetree(qrletree);
	hz_huffman_write_freetree(qvaluetree);
#endif
	//hz_bitstream_write_flushbits(blocks);
	PARANOIDBITS
	//printf("pass %s, byte length %i\n", filterchunknames[filternum], hz_bitstream_write_sizeofblocks(blocks) - a);
}

static void dpi_writeimagergb(char *filename, unsigned char *pixels, int width, int height, int quantize)
{
	int passdatawidth[WAVEBUFFERS], passdataheight[WAVEBUFFERS], i, a, b, x, y, colorindex, w, h, steps, filter, failed, compressbufferlength;
	float *passdata[WAVEBUFFERS][4], o, *in, *out;
	short *compressbuffer;
	//hzhuffmanwritetree_t *rlehufftree, *valuehufftree;
	hz_bitstream_writeblocks_t *blocks, *headerblocks;
	hz_bitstream_write_t *bitstream;

	#define pix(a,x,y,w) ((a)[(y) * (w) + (x)])
	#define bound(min,num,max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

	failed = 0;
	if ((bitstream = hz_bitstream_write_open(filename)) == NULL)
		failed = 1;
	if ((headerblocks = hz_bitstream_write_allocblocks()) == NULL)
		failed = 1;
	if ((blocks = hz_bitstream_write_allocblocks()) == NULL)
		failed = 1;

	passdatawidth[0] = width;
	passdataheight[0] = height;
	compressbufferlength = passdatawidth[0] * passdataheight[0];
	passdata[0][0] = malloc(passdatawidth[0] * passdataheight[0] * sizeof(***passdata));
	passdata[0][1] = NULL;
	passdata[0][2] = NULL;
	passdata[0][3] = NULL;
	if (passdata[0][0] == NULL)
		failed = 1;
	for (i = 1;i < WAVEBUFFERS;i++)
	{
		passdatawidth[i] = ((passdatawidth[i - 1] + WAVELETPREFETCH + 1) >> 1);
		passdataheight[i] = ((passdataheight[i - 1] + WAVELETPREFETCH + 1) >> 1);
		compressbufferlength += passdatawidth[i] * passdataheight[i];
		passdata[i][0] = malloc(passdatawidth[i] * passdataheight[i] * sizeof(***passdata));
		passdata[i][1] = malloc(passdatawidth[i] * passdataheight[i] * sizeof(***passdata));
		passdata[i][2] = malloc(passdatawidth[i] * passdataheight[i] * sizeof(***passdata));
		passdata[i][3] = malloc(passdatawidth[i] * passdataheight[i] * sizeof(***passdata));
		if (passdata[i][0] == NULL || passdata[i][1] == NULL || passdata[i][2] == NULL || passdata[i][3] == NULL)
			failed = 1;
	}

	if ((compressbuffer = malloc((width * height * 3 * 2 + 1000) * sizeof(*compressbuffer))) == NULL)
		failed = 1;

	if (!failed)
	{
		printf("%i by %i image, using %i wavelet transforms (smallest version %ix%i)\n", width, height, WAVESTEPS, width >> WAVESTEPS, height >> WAVESTEPS);
		waveletsetup();
		hz_bitstream_write_bytes(headerblocks, "DPIMAGE", 8);
		hz_bitstream_write_byte(headerblocks, 1);
		hz_bitstream_write_short(headerblocks, width);
		hz_bitstream_write_short(headerblocks, height);
		compressbufferlength = 0;
		for (colorindex = 0;colorindex < 3;colorindex++)
		{
			for (i = 0;i < width * height;i++)
				passdata[0][0][i] = pixels[i * 3 + colorindex];
			for (steps = 0;steps < WAVESTEPS;steps++)
			{
				w = passdatawidth[steps];
				h = passdataheight[steps];
				in = passdata[steps][0];
				for (filter = 0;filter < 4;filter++)
				{
					out = passdata[steps + 1][filter];
					for (y = -WAVELETPREFETCH;y < h;y += 2)
					{
						for (x = -WAVELETPREFETCH;x < w;x += 2)
						{
							o = 0.0f;
							if (colorindex == 1 || filter == 0 || steps > 0)
								for (b = 0;b < WAVESIZE;b++)
									for (a = 0;a < WAVESIZE;a++)
										o += pix(in, bound(0, x + a, (w - 1)), bound(0, y + b, (h - 1)), w) * wave[filter][b][a];
							*out++ = o;
						}
					}
				}
			}
			// now quantize the resulting wavelet transformed data, starting with
			// the smallest lowpass data
			dpi_writequantized(blocks, passdata[WAVESTEPS][0], passdatawidth[WAVESTEPS] * passdataheight[WAVESTEPS], quantize, compressbuffer, 0);
			// then save out the high pass data, smallest to largest
			for (steps = WAVESTEPS;steps >= 1;steps--)
				for (filter = 1;filter < 4;filter++)
					dpi_writequantized(blocks, passdata[steps][filter], passdatawidth[steps] * passdataheight[steps], colorindex == 1 ? quantize : quantize >> 2, compressbuffer, filter);
		}

		/*
		hz_huffman_write_clearcounts(rlehufftree);
		hz_huffman_write_clearcounts(valuehufftree);
		for (i = 0;i < compressbufferlength;)
		{
			hz_huffman_write_countsymbol(rlehufftree, compressbuffer[i++]);
			hz_huffman_write_countsymbol(valuehufftree, compressbuffer[i++]);
		}
		hz_huffman_write_buildtree(rlehufftree);
		hz_huffman_write_buildtree(valuehufftree);
		hz_huffman_write_writetree(blocks, rlehufftree);
		hz_huffman_write_writetree(blocks, valuehufftree);
		for (i = 0;i < compressbufferlength;)
		{
			hz_huffman_write_writesymbol(blocks, rlehufftree, compressbuffer[i++]);
			hz_huffman_write_writesymbol(blocks, valuehufftree, compressbuffer[i++]);
		}
		hz_bitstream_write_flushbits(blocks);
		*/

		hz_bitstream_write_flushbits(blocks);
		hz_bitstream_write_int(headerblocks, hz_bitstream_write_sizeofblocks(blocks));
		hz_bitstream_write_writeblocks(headerblocks, bitstream);
		hz_bitstream_write_writeblocks(blocks, bitstream);
	}

	for (i = 0;i < WAVEBUFFERS;i++)
		for (filter = 0;filter < 4;filter++)
			if (passdata[i][filter])
				free(passdata[i][filter]);

	if (compressbuffer != NULL)
		free(compressbuffer);
	if (blocks != NULL)
		hz_bitstream_write_freeblocks(blocks);
	if (headerblocks != NULL)
		hz_bitstream_write_freeblocks(headerblocks);
	if (bitstream != NULL)
		hz_bitstream_write_close(bitstream);
}





void usage(void)
{
	printf("usage: dpiencoder <inputname.tga> <outputname.dpi> <quality1-256>\n");
}

int main(int argc, char **argv)
{
	tgafile_t *tgafile;
	if (argc != 4)
	{
		usage();
		return 1;
	}
	tgafile = loadtga(argv[1]);
	if (tgafile == NULL)
	{
		printf("unable to open \"%s\", may not be a valid targa file\n", argv[1]);
		return 1;
	}
	dpi_writeimagergb(argv[2], tgafile->data, tgafile->width, tgafile->height, atoi(argv[3]));
	freetga(tgafile);
	return 0;
}
