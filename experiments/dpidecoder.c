
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
static int dpi_decompressimage(dpiinfo_t *dpiinfo, hz_bitstream_readblocks_t *blocks)
{
	int passdatawidth[WAVEBUFFERS], passdataheight[WAVEBUFFERS], i, a, b, x, y, astart, bstart, wsize, hsize, colorindex, w, h, steps, filter, maxcount, vrange, vmin, count, count2, rlebits, valuebits;
	float *passdata[WAVEBUFFERS], o, *in, *out, valuescale, *row, *pix;
	int failed;
#if HUFFMAN
	hzhuffmanreadtree_t *qrletree = NULL;
	hzhuffmanreadtree_t *qvaluetree = NULL;
#endif
//#define CORRUPTIONCHECK {int loop;for (loop = 0;loop < WAVEBUFFERS;loop++) if (passdata[loop][passdatawidth[loop] * passdataheight[loop]] != -999999) Error("corruption detected at %s:%i\n", __FILE__, __LINE__);}
#define CORRUPTIONCHECK
//#define PARANOIDBITS {int blah = hz_bitstream_read_bits(blocks, 32);if (blah != 0xFEFEFEFE) Error("read error %s:%i\n", __FILE__, __LINE__);}
#define PARANOIDBITS

	//printf("%i by %i image, using %i wavelet transforms (smallest version %ix%i)\n", dpiinfo->width, dpiinfo->height, WAVESTEPS, dpiinfo->width >> WAVESTEPS, dpiinfo->height >> WAVESTEPS);
	passdatawidth[0] = dpiinfo->width;
	passdataheight[0] = dpiinfo->height;
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
			//printf("color plane %i\n", colorindex);
			w = passdatawidth[WAVESTEPS];
			h = passdataheight[WAVESTEPS];
			//printf("decoding lowpass\n");
			//printf("file position %i\n", blocks->position + 17);
			PARANOIDBITS
			maxcount = hz_bitstream_read_bits(blocks, 8) + 1;
			vrange = hz_bitstream_read_bits(blocks, 16) + 1;
			vmin = -hz_bitstream_read_bits(blocks, 16);
			valuescale = hz_bitstream_read_bits(blocks, 16);
			for (rlebits = 0;maxcount > (1 << rlebits);rlebits++);
			for (valuebits = 0;vrange > (1 << valuebits);valuebits++);
			#if HUFFMAN
			qrletree = hz_huffman_read_newtree(maxcount);
			qvaluetree = hz_huffman_read_newtree(vrange);
			PARANOIDBITS
			if (hz_huffman_read_readtree(blocks, qrletree))
			{
				Error("qrletree read error\n");
				failed = 1;
				goto aborted;
			}
			PARANOIDBITS
			if (hz_huffman_read_readtree(blocks, qvaluetree))
			{
				Error("qvaluetree read error\n");
				failed = 1;
				goto aborted;
			}
			#endif
			PARANOIDBITS
			for (i = 0;i < w * h;)
			{
				#if HUFFMANREAD
				count = hz_huffman_read_readsymbol(blocks, qrletree) + 1;
				#else
				count = hz_bitstream_read_bits(blocks, rlebits) + 1;
				#endif
				if (count < 1 || count > maxcount)
					printf("invalid count (%i, maxcount %i) on line %i\n", count, maxcount, __LINE__);
				PARANOIDBITS
				#if HUFFMANREAD
				o = hz_huffman_read_readsymbol(blocks, qvaluetree);
				#else
				o = hz_bitstream_read_bits(blocks, valuebits);
				#endif
				if (o < 0 || o >= vrange)
					printf("invalid value on line %i\n", __LINE__);
				o = (o + vmin) * valuescale;
				PARANOIDBITS
				if (count < 1)
					Error("RLE read error (low pass) invalid count %i\n", count);
				if ((i + count) > (w * h))
				{
					Error("RLE read error (low pass)\n");
					failed = 1;
					goto aborted;
				}
				while (count--)
				{
					CORRUPTIONCHECK
					passdata[WAVESTEPS][i++] = o;
					CORRUPTIONCHECK
				}
			}
			#if HUFFMAN
			hz_huffman_read_freetree(qrletree);qrletree = NULL;
			hz_huffman_read_freetree(qvaluetree);qvaluetree = NULL;
			#endif
			//hz_bitstream_read_flushbits(blocks);
			//printf("file position %i\n", blocks->position + 17);
			PARANOIDBITS
			//printf("file position %i\n", blocks->position + 17);
			for (steps = WAVESTEPS;steps;steps--)
			{
				w = passdatawidth[steps - 1];
				h = passdataheight[steps - 1];
				out = passdata[steps - 1];
				CORRUPTIONCHECK
				memset(out, 0, w * h * sizeof(*out));
				CORRUPTIONCHECK

				// only the lowpass filter is fetched from the previous stage
				in = passdata[steps];
				//printf("decoding lowpass %ix%i\n", w, h);
				CORRUPTIONCHECK
				o = 0;
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
						o = *in++;
						if (o)
						{
							wsize = w - x;
							if (wsize > WAVESIZE)
								wsize = WAVESIZE;
							if (x < 0)
								astart = -x;
							else
								astart = 0;
							for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
							{
								for (a = astart;a < wsize;a++)
								{
									CORRUPTIONCHECK
									pix[a] += o * wave[b][a][0];
									CORRUPTIONCHECK
								}
							}
						}
					}
				}
				for (filter = 1;filter < 4;filter++)
				{
					//printf("file position %i\n", blocks->position + 17);
					PARANOIDBITS
					//printf("decoding pass %ix%i, waveform %i\n", w, h, filter);
					maxcount = hz_bitstream_read_bits(blocks, 8) + 1;
					vrange = hz_bitstream_read_bits(blocks, 16) + 1;
					vmin = -hz_bitstream_read_bits(blocks, 16);
					valuescale = hz_bitstream_read_bits(blocks, 16);
					for (rlebits = 0;maxcount > (1 << rlebits);rlebits++);
					for (valuebits = 0;vrange > (1 << valuebits);valuebits++);
					#if HUFFMAN
					qrletree = hz_huffman_read_newtree(maxcount);
					qvaluetree = hz_huffman_read_newtree(vrange);
					PARANOIDBITS
					if (hz_huffman_read_readtree(blocks, qrletree))
					{
						Error("qrletree read error\n");
						failed = 1;
						goto aborted;
					}
					PARANOIDBITS
					if (hz_huffman_read_readtree(blocks, qvaluetree))
					{
						Error("qvaluetree read error\n");
						failed = 1;
						goto aborted;
					}
					#endif
					PARANOIDBITS
					count = 0;
					count2 = 0;
					o = 0;
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
							// high pass filter is RLE decompressed from the stream on-demand
							if (!count)
							{
								#if HUFFMANREAD
								count = hz_huffman_read_readsymbol(blocks, qrletree) + 1;
								#else
								count = hz_bitstream_read_bits(blocks, rlebits) + 1;
								#endif
								if (count < 0 || count > maxcount)
									printf("invalid count on line %i\n", __LINE__);
								PARANOIDBITS
								#if HUFFMANREAD
								o = hz_huffman_read_readsymbol(blocks, qvaluetree);
								#else
								o = hz_bitstream_read_bits(blocks, valuebits);
								#endif
								//printf ("o = %f (%f), c = %i\n", o, (o + vmin) * valuescale, count);
								if (o < 0 || o >= vrange)
									printf("invalid value on line %i\n", __LINE__);
								o = (o + vmin) * valuescale;
								PARANOIDBITS
								//printf("file position %i\n", blocks->position + 17);
								count2 += count;
								if (count2 > (passdatawidth[steps] * passdataheight[steps]))
									Error("RLE read error (high pass decompress)\n");
							}
							count--;
							#if 1
							if (o)
							{
								wsize = w - x;
								if (wsize > WAVESIZE)
									wsize = WAVESIZE;
								if (x < 0)
									astart = -x;
								else
									astart = 0;
								for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
								{
									for (a = astart;a < wsize;a++)
									{
										CORRUPTIONCHECK
										pix[a] += o * wave[b][a][filter];
										CORRUPTIONCHECK
									}
								}
							}
							#else
							if (o)
							{
								for (;x < w && count;x += 2, count--)
								{
									if (o)
									{
										wsize = w - x;
										if (wsize > WAVESIZE)
											wsize = WAVESIZE;
										if (x < 0)
											astart = -x;
										else
											astart = 0;
										for (b = bstart, pix = row + x;b < hsize;b++, pix += w)
											for (a = astart;a < wsize;a++)
												pix[a] += o * wave[b][a][filter];
									}
								}
							}
							else
							{
								a = ((x - w + 1) >> 1);
								if (count < a)
								{
									x += count * 2;
									count = 0;
								}
								else
									count -= a;
							}
							#endif
						}
					}
					#if HUFFMAN
					hz_huffman_read_freetree(qrletree);qrletree = NULL;
					hz_huffman_read_freetree(qvaluetree);qvaluetree = NULL;
					#endif
					//hz_bitstream_read_flushbits(blocks);
					//printf("file position %i\n", blocks->position + 17);
					if (count)
					{
						Error("RLE read error (high pass %i:%i 1)\n", steps, filter);
						failed = 1;
						goto aborted;
					}
					if (count2 != passdatawidth[steps] * passdataheight[steps])
					{
						Error("RLE read error (high pass %i:%i 2)\n", steps, filter);
						failed = 1;
						goto aborted;
					}
					//printf("file position %i\n", blocks->position + 17);
					PARANOIDBITS
				}
			}
			for (i = 0;i < dpiinfo->width * dpiinfo->height;i++)
			{
				a = (int) (passdata[0][i] + 0.5);
				if (a < 0)
					a = 0;
				if (a > 255)
					a = 255;
				CORRUPTIONCHECK
				dpiinfo->pixels[i * 3 + colorindex] = a;
				CORRUPTIONCHECK
			}
		}
	}

	aborted:
	#if HUFFMAN
	hz_huffman_read_freetree(qrletree);
	hz_huffman_read_freetree(qvaluetree);
	#endif
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
