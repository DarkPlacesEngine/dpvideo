
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "tgafile.h"

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

#if 1
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
//#define WAVELETPREFETCH (WAVESIZE)
#define WAVELETPREFETCH (WAVESIZE - 2)

double waveanalysislow [WAVESIZE];
// this is the high pass waveform, made by reversing the order of numbers
// from the wavelow, and negating every even index (0, 2, 4, etc)
// (note: if a different wavelet is used, other methods may be needed - if
//  the wavelet length is odd, negating should be on odd indices rather than
// even indices, and if it is an asymmetric wavelet - different analysis and
// synthesis - more substantial code changes are necessary)
double waveanalysishigh [WAVESIZE];
// these are the same as the analysis
double wavesynthesislow [WAVESIZE];
double wavesynthesishigh [WAVESIZE];

static int linecounter = 0;

static void printnum(FILE *file, double o)
{
	#if 1
	fprintf(file, " %f", o);
	if (++linecounter >= 16)
	{
		fprintf(file, "\n");
		linecounter = 0;
	}
	#endif
}

static void printnumclear(FILE *file)
{
	if (linecounter)
	{
		fprintf(file, "\n");
		linecounter = 0;
	}
}

#define WAVESTEPS 10
static void waveletanalysis(unsigned char *pixels, int width, int height, FILE *waveletfile)
{
	int lowpassdatasize[WAVESTEPS], highpassdatasize[WAVESTEPS], lowpassdatasize2[WAVESTEPS], wavesteps, i, j, colorindex, size, w, wsize, linecounter, steps, offset1, offset2, bestoffset1, bestoffset2;
	float *lowpassdata[WAVESTEPS], *highpassdata[WAVESTEPS], *color[3], f, o, *in, *out;
	double error, besterror;
	for (i = 0;i < WAVESIZE;i++)
	{
		waveanalysislow[i] = wavelet[i];
		waveanalysishigh[i] = wavelet[(WAVESIZE - 1) - i] * ((i & 1) ? -1.0 : 1.0);
		wavesynthesislow[i] = waveanalysislow[i];
		wavesynthesishigh[i] = waveanalysishigh[i];
	}

	color[0] = malloc(width * height * sizeof(**color));
	color[1] = malloc(width * height * sizeof(**color));
	color[2] = malloc(width * height * sizeof(**color));

	wavesteps = WAVESTEPS;
	for (i = 0;i < WAVESTEPS;i++)
	{
		lowpassdatasize2[i] = (width * height) >> i;
		if (lowpassdatasize2[i] <= (WAVESIZE))
		{
			wavesteps = i + 1;
			break;
		}
	}

	lowpassdatasize[0] = width * height;
	highpassdatasize[0] = width * height;
	lowpassdata[0] = malloc(lowpassdatasize[0] * sizeof(**lowpassdata));
	highpassdata[0] = malloc(highpassdatasize[0] * sizeof(**highpassdata));
	for (i = 1;i < wavesteps;i++)
	{
		lowpassdatasize[i] = ((lowpassdatasize[i - 1] + WAVELETPREFETCH + 1) >> 1);
		highpassdatasize[i] = ((highpassdatasize[i - 1] + WAVELETPREFETCH + 1) >> 1);
		lowpassdata[i] = malloc(lowpassdatasize[i] * sizeof(**lowpassdata));
		highpassdata[i] = malloc(highpassdatasize[i] * sizeof(**highpassdata));
	}

	// fill in source data
	for (i = 0;i < width * height;i++)
	{
		color[0][i] = pixels[i * 3 + 0] * (1.0f / 255.0f);
		color[1][i] = pixels[i * 3 + 1] * (1.0f / 255.0f);
		color[2][i] = pixels[i * 3 + 2] * (1.0f / 255.0f);
	}
	besterror = 1000000000.0;
	bestoffset1 = 9999;
	bestoffset2 = 9999;
	offset1 = -WAVELETPREFETCH;
	//for (offset1 = -WAVELETPREFETCH;offset1 <= 0;offset1++)
	{
		offset2 = -WAVELETPREFETCH;
		//for (offset2 = -WAVELETPREFETCH;offset2 <= 0;offset2++)
		{
			//offset1 = -2;
			//offset2 = WAVEHIGHSYNTHSTART;
#define bound(min,num,max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
			linecounter = 0;
			fprintf(waveletfile, "offset1 %i offset2 %i\n", offset1, offset2);
			for (colorindex = 0;colorindex < 1;colorindex++)
			{
				fprintf(waveletfile, "color plane %i\n", colorindex);
				size = width * height;
				memcpy(lowpassdata[0], color[colorindex], width * height * sizeof(**lowpassdata));
				for (steps = 0;steps <= (wavesteps - 2);steps++)
				{
					w = lowpassdatasize[steps];
					in = lowpassdata[steps];

					out = lowpassdata[steps + 1];
					fprintf(waveletfile, "low pass %i:\n", 1 << steps);
					out += WAVELETPREFETCH + offset1;
					for (i = offset1;i < w;i += 2)
					{
						o = 0.0f;
						for (j = 0;j < WAVESIZE;j++)
							o += in[bound(0, i + j, (w - 1))] * waveanalysislow[j];
						*out++ = o;
						printnum(waveletfile, o * 255.0f);
					}
					printnumclear(waveletfile);

					out = highpassdata[steps + 1];
					fprintf(waveletfile, "high pass %i:\n", 1 << steps);
					out += WAVELETPREFETCH + offset2;
					for (i = offset2;i < w;i += 2)
					{
						o = 0.0f;
						for (j = 0;j < WAVESIZE;j++)
							o += in[bound(0, i + j, (w - 1))] * waveanalysishigh[j];
						*out++ = o;
						printnum(waveletfile, o * 255.0f);
					}
					printnumclear(waveletfile);

					//while (out < data[steps + 1] + datasize[steps + 1])
					//	*out++ = 0;
					/*
					// keep the highpass data from the previous passes
					if (w < w2)
						memcpy(out, in + w, (w2 - w) * sizeof(*out));
					*/
				}
				fprintf(waveletfile, "decompression parse:\n");
				for (steps = wavesteps - 2;steps >= 0;steps--)
				{
					w = lowpassdatasize[steps];
					out = lowpassdata[steps];
					for (i = 0;i < w;i++)
						out[i] = 0;
					//memset(out, 0, w * sizeof(*out));

					fprintf(waveletfile, "low pass parse %i:\n", 1 << steps);
					in = lowpassdata[steps + 1];
					in += WAVELETPREFETCH + offset1;
					for (i = offset1;i < w;i += 2)
					{
						o = *in++;
						printnum(waveletfile, o * 255.0f);
						if (i < 0)
						{
							// we know the data is big enough for the wavelet already
							for (j = -i;j < WAVESIZE;j++)
								out[i + j] += o * wavesynthesislow[j];
						}
						else
						{
							wsize = w - i;
							if (wsize > WAVESIZE)
								wsize = WAVESIZE;
							for (j = 0;j < wsize;j++)
								out[i + j] += o * wavesynthesislow[j];
						}
					}
					printnumclear(waveletfile);

					fprintf(waveletfile, "high pass parse %i:\n", 1 << steps);
					in = highpassdata[steps + 1];
					in += WAVELETPREFETCH + offset2;
					for (i = offset2;i < w;i += 2)
					{
						o = *in++;
						printnum(waveletfile, o * 255.0f);
						if (i < 0)
						{
							// we know the data is big enough for the wavelet already
							for (j = -i;j < WAVESIZE;j++)
								out[i + j] += o * wavesynthesishigh[j];
						}
						else
						{
							wsize = w - i;
							if (wsize > WAVESIZE)
								wsize = WAVESIZE;
							for (j = 0;j < wsize;j++)
								out[i + j] += o * wavesynthesishigh[j];
						}
					}
					printnumclear(waveletfile);

					/*
					if (w < w2)
						memcpy(out + w, in, (w2 - w) * sizeof(*out));
					*/
				}
				fprintf(waveletfile, "decompressed:\n");
				linecounter = 0;
				error = 0;
				for (i = 0;i < lowpassdatasize[0];i++)
				{
					j = (int) (lowpassdata[0][i] * 255.0 + 0.5);
					f = (double) j - (double) pixels[i * 3 + colorindex];
					error += f * f;
					#if 1
					fprintf(waveletfile, " %3i:%3i", (int) j, (int) pixels[i * 3 + colorindex]);
					//fprintf(waveletfile, " %3f:%3f", lowpassdata[0][i], pixels[i * 3 + colorindex] * (1.0f / 255.0f));
					if (++linecounter >= 16)
					{
						fprintf(waveletfile, "\n");
						linecounter = 0;
					}
					#endif
				}
				printnumclear(waveletfile);
				error = sqrt(error / (width * height));
				if (besterror >= error)
				{
					besterror = error;
					bestoffset1 = offset1;
					bestoffset2 = offset2;
				}
				printf("error %f offset1 %i offset2 %i\n", error, offset1, offset2);
			}
		}
	}
	printf("best:\nerror %f offset1 %i offset2 %i\n", besterror, bestoffset1, bestoffset2);
}
#else
#define BLOCKSIZEBITS 6
#define BLOCKSIZE (1 << BLOCKSIZEBITS)
static void waveletanalysis(unsigned char *pixels, int width, int height, FILE *waveletfile)
{
	int i, j, a, b, x, y, g, cr, cg, cb, x1, y1, bw, bh, bw2, bh2, best;
	unsigned char *pix;
	int averages[16][4];

	fprintf(waveletfile, "image %dx%d (%dx%d blocks)\n", width, height, (width + BLOCKSIZE - 1) / BLOCKSIZE, (height + BLOCKSIZE - 1) / BLOCKSIZE);
	for (y1 = 0;y1 < height;y1 += BLOCKSIZE)
	{
		bh = BLOCKSIZE;
		if (y1 + bh > height)
			bh = height - y1;
		for (x1 = 0;x1 < width;x1 += BLOCKSIZE)
		{
			bw = BLOCKSIZE;
			if (x1 + bw > width)
				bw = width - x1;

			cr = 0;
			cg = 0;
			cb = 0;
			for (b = 0;b < bh;b++)
			{
				for (a = 0;a < bw;a++)
				{
					pix = pixels + ((y1 + b) * width + (x1 + a)) * 3;
					cr += pix[0];
					cg += pix[1];
					cb += pix[2];
				}
			}
			averages[0][0] = cr >> (BLOCKSIZEBITS * 2);
			averages[0][1] = cg >> (BLOCKSIZEBITS * 2);
			averages[0][2] = cb >> (BLOCKSIZEBITS * 2);
			fprintf(waveletfile, "block%3d,%3d, %3d x%3d, average color:%4d%4d%4d\n", x1 / BLOCKSIZE, y1 / BLOCKSIZE, bw, bh, averages[0][0], averages[0][1], averages[0][2]);

			for (b = 0;b < bh;b++)
			{
				fprintf(waveletfile, "row %d\n", b);
				for (a = 0;a < bw;a++)
				{
					for (best = 0, g = BLOCKSIZE;g;g >>= 1)
					{
						i = x1 + (a & ~(g - 1));
						j = y1 + (b & ~(g - 1));
						bw2 = g;
						if (i + bw2 > width)
							bw2 = width - i;
						bh2 = g;
						if (j + bh2 > height)
							bh2 = height - j;
						cr = 0;
						cg = 0;
						cb = 0;
						for (y = 0;y < bh2;y++)
						{
							for (x = 0;x < bw2;x++)
							{
								pix = pixels + ((j + y) * width + (i + x)) * 3;
								cr += pix[0];
								cg += pix[1];
								cb += pix[2];
							}
						}
						averages[best][0] = cr >> ((BLOCKSIZEBITS - best) * 2);
						averages[best][1] = cg >> ((BLOCKSIZEBITS - best) * 2);
						averages[best][2] = cb >> ((BLOCKSIZEBITS - best) * 2);
						best++;
					}
					for (i = best - 1;i > 0;i--)
					{
						averages[i][0] -= averages[i - 1][0];
						averages[i][1] -= averages[i - 1][1];
						averages[i][2] -= averages[i - 1][2];
					}
					for (i = 1;i < best - 1;i++)
						fprintf(waveletfile, "%4d%4d%4d,", averages[i][0], averages[i][1], averages[i][2]);
					fprintf(waveletfile, "%4d%4d%4d\n", averages[i][0], averages[i][1], averages[i][2]);
				}
			}
		}
	}
	fprintf(waveletfile, "\n\n\n\n");
}
#endif





void usage(void)
{
	printf(
"usage: dpvencoder <name> <framerate> <quality>\n"
"example:\n"
"dpvencoder test 30 20\n"
"would load as many test*.tga frames as are found, named like test00000.tga or\n"
"test0000.tga or test000.tga or test00.tga or even test0.tga, set them up\n"
"for playback at 30 frames per second, and interleave the audio from the\n"
"test.wav file if it exists\n"
"tip: framerate does not need to be integer, 29.97 for NTSC for example\n"
	);
}

int main(int argc, char **argv)
{
	FILE *waveletfile;
	tgafile_t *tgafile;
	if (argc != 2)
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
	waveletfile = fopen("waveletanalysis.txt", "wb");
	waveletanalysis(tgafile->data, tgafile->width, tgafile->height, waveletfile);
	fclose(waveletfile);
	freetga(tgafile);
	return 0;
}
