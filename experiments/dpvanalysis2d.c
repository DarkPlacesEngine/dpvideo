
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
double waveanalysis[4][WAVESIZE][WAVESIZE];
double wavesynthesis[4][WAVESIZE][WAVESIZE];

static void waveletsetup(double newsum)
{
	int x, y, i;
	double sum;
	sum = 0;
	for (y = 0;y < WAVESIZE;y++)
	{
		for (x = 0;x < WAVESIZE;x++)
		{
			waveanalysis[0][y][x] = (wavelet[               x] * wavelet[               y]);
			waveanalysis[1][y][x] = (wavelet[WAVESIZE - 1 - x] * wavelet[               y]) * (((x    ) & 1) ? -1.0 : 1.0);
			waveanalysis[2][y][x] = (wavelet[               x] * wavelet[WAVESIZE - 1 - y]) * (((y    ) & 1) ? -1.0 : 1.0);
			waveanalysis[3][y][x] = (wavelet[WAVESIZE - 1 - x] * wavelet[WAVESIZE - 1 - y]) * (((x + y) & 1) ? -1.0 : 1.0);
			sum += waveanalysis[0][y][x];
		}
	}

	// renormalize to sqrt(2.0) as it should be
	//sum = sqrt(2.0) / sum;
	//printf("sum: %f\n", sum);
	sum = newsum;//12.0;
	if (sum != 1)
	{
		//printf("sum: %f\n", sum);
		for (y = 0;y < WAVESIZE;y++)
			for (x = 0;x < WAVESIZE;x++)
				for (i = 0;i < 4;i++)
					waveanalysis[i][y][x] *= sum;
	}

	// synthesis waveforms are the same as analysis
	for (y = 0;y < WAVESIZE;y++)
	{
		for (x = 0;x < WAVESIZE;x++)
		{
			wavesynthesis[0][y][x] = waveanalysis[0][y][x];
			wavesynthesis[1][y][x] = waveanalysis[1][y][x];
			wavesynthesis[2][y][x] = waveanalysis[2][y][x];
			wavesynthesis[3][y][x] = waveanalysis[3][y][x];
		}
	}
}

static int linecounter = 0;

static void printnum(FILE *file, double o, int n, int maxline)
{
	#if 1
	fprintf(file, " %*g", n, o);
	if (++linecounter >= maxline)
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

#define WAVESTEPS 16
static char *waveformnames[4] = {"lowpass filter", "highpass filter x", "highpass filter y", "highpass filter xy"};
static double waveletanalysis(unsigned char *outputpixels, unsigned char *pixels, int width, int height, int maxsteps, int quantize, double newsum, FILE *waveletfile)
{
	int passdatawidth[WAVESTEPS], passdataheight[WAVESTEPS], wavesteps, i, a, b, x, y, astart, bstart, wsize, hsize, colorindex, w, h, linecounter, steps, filter;
	float *passdata[WAVESTEPS][4], *color[3], f, o, *in, *out, quantizem, quantized;
	double error;

	#define pix(a,x,y,w) ((a)[(y) * (w) + (x)])
	#define bound(min,num,max) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))
	color[0] = malloc(width * height * sizeof(**color));
	color[1] = malloc(width * height * sizeof(**color));
	color[2] = malloc(width * height * sizeof(**color));
	// fill in source data
	for (i = 0;i < width * height;i++)
	{
		color[0][i] = pixels[i * 3 + 0];
		color[1][i] = pixels[i * 3 + 1];
		color[2][i] = pixels[i * 3 + 2];
	}

	//for (i = 0;(width >> i) >= 2 && (height >> i) >= 2;i++)
	//	wavesteps = i + 1;
	//if (wavesteps > maxsteps)
		wavesteps = maxsteps;

	printf("%i by %i image, using %i wavelet transforms (smallest version %ix%i)\n", width, height, wavesteps - 1, width >> (wavesteps - 1), height >> (wavesteps - 1));

	passdatawidth[0] = width;
	passdataheight[0] = height;
	passdata[0][0] = malloc(passdatawidth[0] * passdataheight[0] * sizeof(***passdata));
	passdata[0][1] = NULL;
	passdata[0][2] = NULL;
	passdata[0][3] = NULL;
	for (i = 1;i < wavesteps;i++)
	{
		passdatawidth[i] = ((passdatawidth[i - 1] + WAVELETPREFETCH + 1) >> 1);
		passdataheight[i] = ((passdataheight[i - 1] + WAVELETPREFETCH + 1) >> 1);
		passdata[i][0] = malloc(passdatawidth[i] * passdataheight[i] * sizeof(***passdata));
		passdata[i][1] = malloc(passdatawidth[i] * passdataheight[i] * sizeof(***passdata));
		passdata[i][2] = malloc(passdatawidth[i] * passdataheight[i] * sizeof(***passdata));
		passdata[i][3] = malloc(passdatawidth[i] * passdataheight[i] * sizeof(***passdata));
	}

	waveletsetup(newsum);
	linecounter = 0;
	for (colorindex = 0;colorindex < 3;colorindex++)
	{
		fprintf(waveletfile, "color plane %i\n", colorindex);
		memcpy(passdata[0][0], color[colorindex], width * height * sizeof(***passdata));
		for (steps = 0;steps <= (wavesteps - 2);steps++)
		{
			w = passdatawidth[steps];
			h = passdataheight[steps];
			in = passdata[steps][0];

			for (filter = 0;filter < 4;filter++)
			{
				out = passdata[steps + 1][filter];
				//fprintf(waveletfile, "encoding pass %ix%i, waveform %s:\n", 1 << (steps + 1), 1 << (steps + 1), waveformnames[filter]);
				for (y = -WAVELETPREFETCH;y < h;y += 2)
				{
					for (x = -WAVELETPREFETCH;x < w;x += 2)
					{
						o = 0.0f;
						for (b = 0;b < WAVESIZE;b++)
							for (a = 0;a < WAVESIZE;a++)
								o += pix(in, bound(0, x + a, (w - 1)), bound(0, y + b, (h - 1)), w) * waveanalysis[filter][b][a];
						if (out >= (passdata[steps + 1][filter] + passdatawidth[steps + 1] * passdataheight[steps + 1]))
						{
							printf("compression error: write overrun\n");
							return 0;
						}
						*out++ = o;
						//printnum(waveletfile, o);
					}
				}
				//printf("out result %i, should be %i\n", out - passdata[steps + 1][filter], passdatawidth[steps + 1] * passdataheight[steps + 1]);
				//printnumclear(waveletfile);
			}
		}
		if (quantize)
		{
			for (steps = 1;steps < wavesteps;steps++)
			{
				quantizem = quantize;
				if (quantizem > 256)
					quantizem = 256;
				quantizem *= (1.0 / 256.0);
				quantized = 1.0 / quantizem;
				for (filter = 0;filter < 4;filter++)
				{
					if (filter || steps == wavesteps - 1)
					{
						fprintf(waveletfile, "quantized pass %ix%i, waveform %s:\n", 1 << steps, 1 << steps, waveformnames[filter]);
						for (i = 0;i < passdatawidth[steps] * passdataheight[steps];i++)
						{
							o = passdata[steps][filter][i] * quantizem;
							if (o < 0)
								o = (int)(o - 0.5);
							else
								o = (int)(o + 0.5);
							printnum(waveletfile, o, 5, passdatawidth[steps]);
							passdata[steps][filter][i] = o * quantized;
						}
						printnumclear(waveletfile);
					}
				}
			}
		}
		fprintf(waveletfile, "decompression parse:\n");
		for (steps = wavesteps - 2;steps >= 0;steps--)
		{
			w = passdatawidth[steps];
			h = passdataheight[steps];
			out = passdata[steps][0];
			memset(out, 0, w * h * sizeof(*out));

			for (filter = 0;filter < 4;filter++)
			{
				//fprintf(waveletfile, "decoding pass %ix%i, waveform %s:\n", 1 << (steps + 1), 1 << (steps + 1), waveformnames[filter]);
				in = passdata[steps + 1][filter];
				for (y = -WAVELETPREFETCH;y < h;y += 2)
				{
					hsize = h - y;
					if (hsize > WAVESIZE)
						hsize = WAVESIZE;
					if (y < 0)
						bstart = -y;
					else
						bstart = 0;

					for (x = -WAVELETPREFETCH;x < w;x += 2)
					{
						if (in >= (passdata[steps + 1][filter] + passdatawidth[steps + 1] * passdataheight[steps + 1]))
						{
							printf("decompression error: read overrun\n");
							return 0;
						}
						o = *in++;
						//if (filter)
						//	printnum(waveletfile, o, 5, (w + 1) >> 1);
						if (o)
						{
							wsize = w - x;
							if (wsize > WAVESIZE)
								wsize = WAVESIZE;
							if (x < 0)
								astart = -x;
							else
								astart = 0;

							for (b = bstart;b < hsize;b++)
								for (a = astart;a < wsize;a++)
									pix(out, x + a, y + b, w) += o * wavesynthesis[filter][b][a];
						}
					}
				}
				//printnumclear(waveletfile);
				//printf("in result %i, should be %i\n", in - passdata[steps + 1][filter], passdatawidth[steps + 1] * passdataheight[steps + 1]);
			}
		}
		fprintf(waveletfile, "decompressed:\n");
		linecounter = 0;
		error = 0;
		for (i = 0;i < width * height;i++)
		{
			a = (int) (passdata[0][0][i] + 0.5);
			if (a < 0)
				a = 0;
			if (a > 255)
				a = 255;
			if (outputpixels != NULL)
				outputpixels[i * 3 + colorindex] = a;
			f = (double) a - (double) pixels[i * 3 + colorindex];
			error += f * f;
			#if 1
			fprintf(waveletfile, " %3i:%3i", (int) a, (int) pixels[i * 3 + colorindex]);
			if (++linecounter >= width)
			{
				fprintf(waveletfile, "\n");
				linecounter = 0;
			}
			#endif
		}
		printnumclear(waveletfile);
		error = sqrt(error / (width * height));
		printf("degradation error %f\n", error);
	}

	for (i = 0;i < wavesteps;i++)
	{
		free(passdata[i][0]);
		free(passdata[i][1]);
		free(passdata[i][2]);
		free(passdata[i][3]);
	}
	free(color[0]);
	free(color[1]);
	free(color[2]);

	return error;
}





void usage(void)
{
	printf("usage: dpvanalysis2d <name> <steps> <quantize>\n");
}

int main(int argc, char **argv)
{
	FILE *waveletfile;
	tgafile_t *tgafile;
	void *output;
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
	waveletfile = fopen("waveletanalysis.txt", "wb");
	if (waveletfile == NULL)
	{
		printf("unable to open report file\n");
		freetga(tgafile);
		return 1;
	}
	output = malloc(tgafile->width * tgafile->height * 3);
	if (output == NULL)
	{
		printf("unable to allocate memory\n");
		fclose(waveletfile);
		freetga(tgafile);
		return 1;
	}
#if 0
	{
		double sum1;
		double sum2;
		double sum3;
		double sum4;
		double sum5;
		double error1;
		double error2;
		double error3;
		double error4;
		double error5;
		sum1 = 0.1;
		sum2 = 100;
		error1 = waveletanalysis(NULL, tgafile->data, tgafile->width, tgafile->height, atoi(argv[2]), atoi(argv[3]), sum1, waveletfile);
		error2 = waveletanalysis(NULL, tgafile->data, tgafile->width, tgafile->height, atoi(argv[2]), atoi(argv[3]), sum2, waveletfile);
		while (error1 > 0.1 || error2 > 0.1)
		{
			printf("best sums: %f (error %f), %f (error %f)\n", sum1, error1, sum2, error2);
			sum3 = sum1 * 0.75 + sum2 * 0.25;
			sum4 = sum1 * 0.50 + sum2 * 0.50;
			sum5 = sum1 * 0.25 + sum2 * 0.75;
			error3 = waveletanalysis(NULL, tgafile->data, tgafile->width, tgafile->height, atoi(argv[2]), atoi(argv[3]), sum3, waveletfile);
			error4 = waveletanalysis(NULL, tgafile->data, tgafile->width, tgafile->height, atoi(argv[2]), atoi(argv[3]), sum4, waveletfile);
			error5 = waveletanalysis(NULL, tgafile->data, tgafile->width, tgafile->height, atoi(argv[2]), atoi(argv[3]), sum5, waveletfile);
			if (error3 <= error4 && error3 <= error5)
			{
				sum2 = sum4;
				error2 = error4;
			}
			else if (error5 <= error3 && error5 <= error4)
			{
				sum1 = sum4;
				error1 = error4;
			}
			else if (error4 <= error3 && error4 <= error5)
			{
				sum1 = sum3;
				sum2 = sum5;
				error1 = error3;
				error2 = error5;
			}
		}
	}
#else
	waveletanalysis(output, tgafile->data, tgafile->width, tgafile->height, atoi(argv[2]) + 1, atoi(argv[3]), 1, waveletfile);
	savetga_rgb24_topdown("wavelet.tga", output, tgafile->width, tgafile->height);
#endif
	free(output);
	fclose(waveletfile);
	freetga(tgafile);
	return 0;
}
