
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
static void dpi_writequantized(hz_bitstream_writeblocks_t *blocks, float *data, int length, int quantize1, int quantize2, int quantize3, short *compressbuffer, int filternum)
{
	int i, a1, a2, a3, n1, n2, n3, count, compressbufferlength, vmin1, vmin2, vmin3, vmax1, vmax2, vmax3, vrange1, vrange2, vrange3, maxcount, rlebits, valuebits1, valuebits2, valuebits3;
	float q1, q2, q3, iq1, iq2, iq3;
#if HUFFMAN
	hzhuffmanwritetree_t *qrletree, *qvaluetree1, *qvaluetree2, *qvaluetree3;
#endif
	n1 = n2 = n3 = 2000000000;
	count = 0;
	compressbufferlength = 0;
	if (quantize1 > 256)
		quantize1 = 256;
	if (quantize1 < 1)
		quantize1 = 1;
	if (quantize2 > 256)
		quantize2 = 256;
	if (quantize2 < 1)
		quantize2 = 1;
	if (quantize3 > 256)
		quantize3 = 256;
	if (quantize3 < 1)
		quantize3 = 1;
	q1 = quantize1 * (1.0 / 256.0);
	iq1 = 1.0 / q1;
	q2 = quantize2 * (1.0 / 256.0);
	iq2 = 1.0 / q2;
	q3 = quantize3 * (1.0 / 256.0);
	iq3 = 1.0 / q3;
	vmin1 = 0;
	vmax1 = 0;
	vmin2 = 0;
	vmax2 = 0;
	vmin3 = 0;
	vmax3 = 0;
	maxcount = 1;
	for (i = 0;i < length;i++)
	{
		a1 = (int)(data[i * 3 + 0] * q1);
		a2 = (int)(data[i * 3 + 1] * q2);
		a3 = (int)(data[i * 3 + 2] * q3);
		if (vmin1 > a1)
			vmin1 = a1;
		if (vmax1 < a1)
			vmax1 = a1;
		if (vmin2 > a2)
			vmin2 = a2;
		if (vmax2 < a2)
			vmax2 = a2;
		if (vmin3 > a3)
			vmin3 = a3;
		if (vmax3 < a3)
			vmax3 = a3;
#if 1
		if (n1 != a1 || n2 != a2 || n3 != a3 || count >= 256)
		{
			if (count)
			{
				if (maxcount < count)
					maxcount = count;
				compressbuffer[compressbufferlength++] = count - 1;
				compressbuffer[compressbufferlength++] = n1;
				compressbuffer[compressbufferlength++] = n2;
				compressbuffer[compressbufferlength++] = n3;
			}
			n1 = a1;
			n2 = a2;
			n3 = a3;
			count = 0;
		}
		count++;
#else
		compressbuffer[compressbufferlength++] = 0;
		compressbuffer[compressbufferlength++] = a1;
		compressbuffer[compressbufferlength++] = a2;
		compressbuffer[compressbufferlength++] = a3;
#endif
	}
	if (count)
	{
		if (maxcount < count)
			maxcount = count;
		compressbuffer[compressbufferlength++] = count - 1;
		compressbuffer[compressbufferlength++] = n1;
		compressbuffer[compressbufferlength++] = n2;
		compressbuffer[compressbufferlength++] = n3;
	}

	vrange1 = vmax1 - vmin1 + 1;
	vrange2 = vmax2 - vmin2 + 1;
	vrange3 = vmax3 - vmin3 + 1;
	for (rlebits = 0;maxcount > (1 << rlebits);rlebits++);
	for (valuebits1 = 0;vrange1 > (1 << valuebits1);valuebits1++);
	for (valuebits2 = 0;vrange2 > (1 << valuebits2);valuebits2++);
	for (valuebits3 = 0;vrange3 > (1 << valuebits3);valuebits3++);
	//rlebits = 0;
	//valuebits = 16;
	//a = hz_bitstream_write_sizeofblocks(blocks);
	//hz_bitstream_write_bytes(blocks, filterchunknames[filternum], 8);
	hz_bitstream_write_bits(blocks, maxcount - 1, 8);
	hz_bitstream_write_bits(blocks, vrange1 - 1, 16);
	hz_bitstream_write_bits(blocks, vrange2 - 1, 16);
	hz_bitstream_write_bits(blocks, vrange3 - 1, 16);
	hz_bitstream_write_bits(blocks, -vmin1, 16);
	hz_bitstream_write_bits(blocks, -vmin2, 16);
	hz_bitstream_write_bits(blocks, -vmin3, 16);
	hz_bitstream_write_bits(blocks, (int) iq1, 16);
	hz_bitstream_write_bits(blocks, (int) iq2, 16);
	hz_bitstream_write_bits(blocks, (int) iq3, 16);
#if HUFFMAN
	qrletree = hz_huffman_write_newtree(maxcount);
	qvaluetree1 = hz_huffman_write_newtree(vrange1);
	qvaluetree2 = hz_huffman_write_newtree(vrange2);
	qvaluetree3 = hz_huffman_write_newtree(vrange3);
	hz_huffman_write_clearcounts(qrletree);
	hz_huffman_write_clearcounts(qvaluetree1);
	hz_huffman_write_clearcounts(qvaluetree2);
	hz_huffman_write_clearcounts(qvaluetree3);
	//for (i = 0;i < maxcount;i++)
	//	hz_huffman_write_countsymbol(qrletree, i);
	//for (i = 0;i < vrange1;i++)
	//	hz_huffman_write_countsymbol(qvaluetree1, i);
	//for (i = 0;i < vrange2;i++)
	//	hz_huffman_write_countsymbol(qvaluetree2, i);
	//for (i = 0;i < vrange3;i++)
	//	hz_huffman_write_countsymbol(qvaluetree3, i);
	for (i = 0;i < compressbufferlength;i += 4)
	{
		hz_huffman_write_countsymbol(qrletree, compressbuffer[i + 0]);
		hz_huffman_write_countsymbol(qvaluetree1, compressbuffer[i + 1] - vmin1);
		hz_huffman_write_countsymbol(qvaluetree2, compressbuffer[i + 2] - vmin2);
		hz_huffman_write_countsymbol(qvaluetree3, compressbuffer[i + 3] - vmin3);
	}
	hz_huffman_write_buildtree(qrletree);
	hz_huffman_write_buildtree(qvaluetree1);
	hz_huffman_write_buildtree(qvaluetree2);
	hz_huffman_write_buildtree(qvaluetree3);
	hz_huffman_write_writetree(blocks, qrletree);
	hz_huffman_write_writetree(blocks, qvaluetree1);
	hz_huffman_write_writetree(blocks, qvaluetree2);
	hz_huffman_write_writetree(blocks, qvaluetree3);
#endif
	for (i = 0;i < compressbufferlength;i += 4)
	{
		if (compressbuffer[i + 1] < vmin1 || compressbuffer[i + 1] > vmax1)
			printf("invalid value %i (outside range %i to %i)\n", compressbuffer[i + 1], vmin1, vmax1);
		if (compressbuffer[i + 2] < vmin2 || compressbuffer[i + 2] > vmax2)
			printf("invalid value %i (outside range %i to %i)\n", compressbuffer[i + 2], vmin2, vmax2);
		if (compressbuffer[i + 3] < vmin3 || compressbuffer[i + 3] > vmax3)
			printf("invalid value %i (outside range %i to %i)\n", compressbuffer[i + 3], vmin3, vmax3);
#if HUFFMANWRITE
		hz_huffman_write_writesymbol(blocks, qrletree, compressbuffer[i + 0]);
		hz_huffman_write_writesymbol(blocks, qvaluetree1, compressbuffer[i + 1] - vmin1);
		hz_huffman_write_writesymbol(blocks, qvaluetree2, compressbuffer[i + 2] - vmin2);
		hz_huffman_write_writesymbol(blocks, qvaluetree3, compressbuffer[i + 3] - vmin3);
#else
		hz_bitstream_write_bits(blocks, compressbuffer[i + 0], rlebits);
		hz_bitstream_write_bits(blocks, compressbuffer[i + 1] - vmin1, valuebits1);
		hz_bitstream_write_bits(blocks, compressbuffer[i + 2] - vmin2, valuebits2);
		hz_bitstream_write_bits(blocks, compressbuffer[i + 3] - vmin3, valuebits3);
#endif
	}
#if HUFFMAN
	hz_huffman_write_freetree(qrletree);
	hz_huffman_write_freetree(qvaluetree1);
	hz_huffman_write_freetree(qvaluetree2);
	hz_huffman_write_freetree(qvaluetree3);
#endif
}

static void dpi_writeimagergb(char *filename, unsigned char *pixels, int width, int height, int quantize)
{
	int passdatawidth[WAVEBUFFERS], passdataheight[WAVEBUFFERS], i, a, b, x, y, w, h, steps, filter, failed, compressbufferlength;
	float *passdata[WAVEBUFFERS][4], o1, o2, o3, *in, *out;
	short *compressbuffer;
	hz_bitstream_writeblocks_t *blocks, *headerblocks;
	hz_bitstream_write_t *bitstream;

	#define pix(a,x,y,c,w) ((a)[((y) * (w) + (x)) * 3 + (c)])
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
	//compressbufferlength = passdatawidth[0] * passdataheight[0];
	passdata[0][0] = malloc(passdatawidth[0] * passdataheight[0] * 3 * sizeof(***passdata));
	passdata[0][1] = NULL;
	passdata[0][2] = NULL;
	passdata[0][3] = NULL;
	if (passdata[0][0] == NULL)
		failed = 1;
	for (i = 1;i < WAVEBUFFERS;i++)
	{
		passdatawidth[i] = ((passdatawidth[i - 1] + WAVELETPREFETCH + 1) >> 1);
		passdataheight[i] = ((passdataheight[i - 1] + WAVELETPREFETCH + 1) >> 1);
		//compressbufferlength += passdatawidth[i] * passdataheight[i];
		passdata[i][0] = malloc(passdatawidth[i] * passdataheight[i] * 3 * sizeof(***passdata));
		passdata[i][1] = malloc(passdatawidth[i] * passdataheight[i] * 3 * sizeof(***passdata));
		passdata[i][2] = malloc(passdatawidth[i] * passdataheight[i] * 3 * sizeof(***passdata));
		passdata[i][3] = malloc(passdatawidth[i] * passdataheight[i] * 3 * sizeof(***passdata));
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
		for (i = 0;i < width * height * 3;i++)
			passdata[0][0][i] = pixels[i];
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
						o1 = 0.0f;
						o2 = 0.0f;
						o3 = 0.0f;
						for (b = 0;b < WAVESIZE;b++)
						{
							for (a = 0;a < WAVESIZE;a++)
							{
								o1 += pix(in, bound(0, x + a, (w - 1)), bound(0, y + b, (h - 1)), 0, w) * wave[filter][b][a];
								o2 += pix(in, bound(0, x + a, (w - 1)), bound(0, y + b, (h - 1)), 1, w) * wave[filter][b][a];
								o3 += pix(in, bound(0, x + a, (w - 1)), bound(0, y + b, (h - 1)), 2, w) * wave[filter][b][a];
							}
						}
						*out++ = o1;
						*out++ = o2;
						*out++ = o3;
					}
				}
			}
		}
		// now quantize the resulting wavelet transformed data, starting with
		// the smallest lowpass data
		dpi_writequantized(blocks, passdata[WAVESTEPS][0], passdatawidth[WAVESTEPS] * passdataheight[WAVESTEPS], quantize, quantize, quantize, compressbuffer, 0);
		// then save out the high pass data, smallest to largest
		for (steps = WAVESTEPS;steps >= 1;steps--)
			for (filter = 1;filter < 4;filter++)
				//dpi_writequantized(blocks, passdata[steps][filter], passdatawidth[steps] * passdataheight[steps], quantize, quantize, quantize, compressbuffer, filter);
				//dpi_writequantized(blocks, passdata[steps][filter], passdatawidth[steps] * passdataheight[steps], quantize >> 2, quantize, quantize >> 2, compressbuffer, filter);
				//dpi_writequantized(blocks, passdata[steps][filter], passdatawidth[steps] * passdataheight[steps], steps > 1 ? quantize >> 2 : 0, quantize, steps > 1 ? quantize >> 2 : 0, compressbuffer, filter);
				dpi_writequantized(blocks, passdata[steps][filter], passdatawidth[steps] * passdataheight[steps], steps > 1 ? quantize : 0, quantize, steps > 1 ? quantize : 0, compressbuffer, filter);
				//dpi_writequantized(blocks, passdata[steps][filter], passdatawidth[steps] * passdataheight[steps], steps > 1 ? quantize : quantize >> 2, quantize, steps > 1 ? quantize : quantize >> 2, compressbuffer, filter);

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
