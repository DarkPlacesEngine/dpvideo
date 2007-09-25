
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "dpvencode.h"
#include "hz_write.h"

#include "hz_write.c"

#define BUFFERSIZE 65536
#define BLOCKSIZE 8

typedef struct dpvencodeblockinfo_s
{
	unsigned char deltacount;
}
dpvencodeblockinfo_t;

typedef struct dpvencodestream_s
{
	// consistent stream properties
	unsigned int width, height;
	unsigned int blockswidth, blocksheight;
	double framerate;
	double videoquality;
	//double soundquality;

	// table of sound delta values
	//int soundtable[64];

	// output file
	//FILE *file;
	hz_bitstream_write_t *bitstream;
	hz_bitstream_writeblocks_t *headerblocks;
	hz_bitstream_writeblocks_t *frameblocks;
	//hzhuffmanwritetree_t *audiohuffmantree;

	// width * height pixels, in RGB
	// this is used for delta compression of blocks
	unsigned char *videopixels;

	dpvencodeblockinfo_t *blockinfo;

	// current position information
	unsigned int videoframenum;
	//unsigned int audioframenum;

	// 0 unless there is an error
	int error;
}
dpvencodestream_t;

/*
static void dpvencode_buildsoundtable(dpvencodestream_t *s)
{
	int i, k;
	double f;
	for (i = 0;i < 64;i++)
	{
		k = i;
		if (k >= 32)
			k = 32 - (k - 32);
		f = k * (1.0 / 32.0);
		s->soundtable[i] = (int) (pow(f, 2) * 32768.0 * (i >= 32 ? -1.0 : 1.0));
	}
}

static int dpvencode_countsoundsample(dpvencodestream_t *s, int n)
{
	int i, bi;

	if (n < 0)
	{
		bi = 0;
		for (i = 63;i >= 32;i--)
		{
			if (s->soundtable[i] < n)
				break;
			bi = i;
		}
	}
	else
	{
		bi = 0;
		for (i = 1;i < 32;i++)
		{
			if (s->soundtable[i] > n)
				break;
			bi = i;
		}
	}

*/
	/*
	// this could cause clipping errors (writing a delta too big)
	bi = 0;
	bd = s->soundtable[bi] - n;
	bd *= bd;
	for (i = 1;i < 64;i++)
	{
		d = s->soundtable[i] - n;
		d *= d;
		if (bd > d)
		{
			bi = i;
			bd = d;
		}
	}
	*/
/*

	hz_huffman_write_countsymbol(s->audiohuffmantree, bi);
	return s->soundtable[bi];
}

static int dpvencode_writesoundsample(dpvencodestream_t *s, int n)
{
	int i, bi;

	if (n < 0)
	{
		bi = 0;
		for (i = 63;i >= 32;i--)
		{
			if (s->soundtable[i] < n)
				break;
			bi = i;
		}
	}
	else
	{
		bi = 0;
		for (i = 1;i < 32;i++)
		{
			if (s->soundtable[i] > n)
				break;
			bi = i;
		}
	}

*/
	/*
	// this could cause clipping errors (writing a delta too big)
	bi = 0;
	bd = s->soundtable[bi] - n;
	bd *= bd;
	for (i = 1;i < 64;i++)
	{
		d = s->soundtable[i] - n;
		d *= d;
		if (bd > d)
		{
			bi = i;
			bd = d;
		}
	}
	*/
/*

	hz_huffman_write_writesymbol(s->frameblocks, s->audiohuffmantree, bi);
	return s->soundtable[bi];
}

static void dpvencode_sound(dpvencodestream_t *s, short *sound, unsigned int soundlen, double quality)
{
	int i, m, l, mp, lp;

	hz_huffman_write_clearcounts(s->audiohuffmantree);
	mp = 0;
	lp = 0;
	for (i = 0;i < soundlen;i++)
	{
		m = (sound[0] + sound[1]) >> 1;
		l = sound[0] - m;
		sound += 2;
		mp += dpvencode_countsoundsample(s, m - mp);
		lp += dpvencode_countsoundsample(s, l - lp);
	}
	hz_huffman_write_buildtree(s->audiohuffmantree);

	hz_huffman_write_writetree(s->frameblocks, s->audiohuffmantree);
	mp = 0;
	lp = 0;
	for (i = 0;i < soundlen;i++)
	{
		m = (sound[0] + sound[1]) >> 1;
		l = sound[0] - m;
		sound += 2;
		mp += dpvencode_writesoundsample(s, m - mp);
		lp += dpvencode_writesoundsample(s, l - lp);
	}
}
*/

static void dpvencode_compressimage(dpvencodestream_t *s, unsigned char *pixels, double quality)
{
	int i, j, a, b, cr, cg, cb, x1, y1, width, height, bw, bh, palettebits, paletteuse[4096], best1, best2, best, bestscore, score, colors, rdist, gdist, bdist, threshold, deltablocks, deltablockthreshold;
	int deltapixelerror, deltablockerror = 0;
	unsigned char palette[4096 * 4], *pix, *p;
	double error, errorstart, errorblock, errorbiggest;
	dpvencodeblockinfo_t *block;

	width = s->width;
	height = s->height;
	error = ((256.0 / 100.0) / quality);
	threshold = (int) (error * error);
	deltablockthreshold = (int) (BLOCKSIZE * BLOCKSIZE * threshold / 8.0);
	error = 0;
	errorbiggest = 0;
	deltablocks = 0;
	block = s->blockinfo;
	for (y1 = 0;y1 < height;y1 += BLOCKSIZE)
	{
		bh = BLOCKSIZE;
		if (y1 + bh > height)
			bh = height - y1;
		for (x1 = 0;x1 < width;x1 += BLOCKSIZE, block++)
		{
			bw = BLOCKSIZE;
			if (x1 + bw > width)
				bw = width - x1;
			errorstart = error;
			if (block->deltacount > 0)
			{
				deltablockerror = 0;
				deltapixelerror = 0;
				for (b = 0;b < bh;b++)
				{
					for (a = 0;a < bw;a++)
					{
						pix = pixels + ((y1 + b) * width + (x1 + a)) * 3;
						cr = pix[0];
						cg = pix[1];
						cb = pix[2];
						p = s->videopixels + ((y1 + b) * width + (x1 + a)) * 3;
						cr -= p[0];
						cg -= p[1];
						cb -= p[2];
						best1 = cr * cr + cg * cg + cb * cb;
						if (deltapixelerror < best1)
							deltapixelerror = best1;
						deltablockerror += best1;
					}
				}
				deltablockerror += deltapixelerror * BLOCKSIZE * BLOCKSIZE;
			}
			//if ((deltaerror / (bw * bh)) <= (threshold >> 4) && block->deltacount > 0)
			if (block->deltacount > 0 && deltablockerror <= deltablockthreshold)
			{
				error += deltablockerror;
				hz_bitstream_write_bit(s->frameblocks, 0);
				block->deltacount--;
				deltablocks++;
			}
			else
			{
				hz_bitstream_write_bit(s->frameblocks, 1);
				block->deltacount = 29;
				colors = 0;
				for (b = 0;b < bh;b++)
				{
					for (a = 0;a < bw;a++)
					{
						pix = pixels + ((y1 + b) * width + (x1 + a)) * 3;
						cr = pix[0];
						cg = pix[1];
						cb = pix[2];
						for (i = 0, p = palette;i < colors;i++, p += 3)
							if (p[0] == cr && p[1] == cg && p[2] == cb)
								break;
						if (i >= colors)
						{
							paletteuse[colors] = 1;
							p[0] = cr;
							p[1] = cg;
							p[2] = cb;
							colors++;
						}
						else
						{
							paletteuse[i]++;
							// keep most common matchs at front of list
							if (i != 0/* && paletteuse[i] > paletteuse[0]*/)
							{
								cr = palette[0];
								cg = palette[1];
								cb = palette[2];
								score = paletteuse[0];
								palette[0] = p[0];
								palette[1] = p[1];
								palette[2] = p[2];
								paletteuse[0] = paletteuse[i];
								palette[0] = cr;
								palette[1] = cg;
								palette[2] = cb;
								paletteuse[i] = score;
							}
						}
					}
				}
				while (colors > 1)
				{
					best1 = -1;
					best2 = -1;
					bestscore = 1000000000;
					for (i = 0;i < colors;i++)
					{
						for (j = 0;j < colors;j++)
						{
							if (j != i)
							{
								rdist = palette[i * 3 + 0] - palette[j * 3 + 0];
								gdist = palette[i * 3 + 1] - palette[j * 3 + 1];
								bdist = palette[i * 3 + 2] - palette[j * 3 + 2];
								score = (rdist * rdist + gdist * gdist + bdist * bdist) * (paletteuse[j] + paletteuse[i]);
								if (bestscore > score)
								{
									best1 = i;
									best2 = j;
									bestscore = score;
								}
							}
						}
					}
					if (bestscore >= threshold && colors <= 128)
						break;
					palette[best1 * 3 + 0] = (unsigned char) (((unsigned int) palette[best1 * 3 + 0] + (unsigned int) palette[best2 * 3 + 0]) >> 1);
					palette[best1 * 3 + 1] = (unsigned char) (((unsigned int) palette[best1 * 3 + 1] + (unsigned int) palette[best2 * 3 + 1]) >> 1);
					palette[best1 * 3 + 2] = (unsigned char) (((unsigned int) palette[best1 * 3 + 2] + (unsigned int) palette[best2 * 3 + 2]) >> 1);
					paletteuse[best1] += paletteuse[best2];
					colors--;
					palette[best2 * 3 + 0] = palette[colors * 3 + 0];
					palette[best2 * 3 + 1] = palette[colors * 3 + 1];
					palette[best2 * 3 + 2] = palette[colors * 3 + 2];
					paletteuse[best2] = paletteuse[colors];
				}
				for (palettebits = 0;(1 << palettebits) < colors;palettebits++);
				if ((1 << palettebits) != colors)
				{
					colors = 0;
					for (b = 0;b < bh;b++)
					{
						for (a = 0;a < bw;a++)
						{
							pix = pixels + ((y1 + b) * width + (x1 + a)) * 3;
							cr = pix[0];
							cg = pix[1];
							cb = pix[2];
							for (i = 0, p = palette;i < colors;i++, p += 3)
								if (p[0] == cr && p[1] == cg && p[2] == cb)
									break;
							if (i >= colors)
							{
								paletteuse[colors] = 1;
								p[0] = cr;
								p[1] = cg;
								p[2] = cb;
								colors++;
							}
							else
							{
								paletteuse[i]++;
								// keep most common matchs at front of list
								if (i != 0/* && paletteuse[i] > paletteuse[0]*/)
								{
									cr = palette[0];
									cg = palette[1];
									cb = palette[2];
									score = paletteuse[0];
									palette[0] = p[0];
									palette[1] = p[1];
									palette[2] = p[2];
									paletteuse[0] = paletteuse[i];
									palette[0] = cr;
									palette[1] = cg;
									palette[2] = cb;
									paletteuse[i] = score;
								}
							}
						}
					}
					while (colors > (1 << palettebits))
					{
						best1 = -1;
						best2 = -1;
						bestscore = 1000000000;
						for (i = 0;i < colors;i++)
						{
							for (j = 0;j < colors;j++)
							{
								if (j != i)
								{
									rdist = palette[i * 3 + 0] - palette[j * 3 + 0];
									gdist = palette[i * 3 + 1] - palette[j * 3 + 1];
									bdist = palette[i * 3 + 2] - palette[j * 3 + 2];
									score = (rdist * rdist + gdist * gdist + bdist * bdist) * (paletteuse[j] + paletteuse[i]);
									if (bestscore > score)
									{
										best1 = i;
										best2 = j;
										bestscore = score;
									}
								}
							}
						}
						palette[best1 * 3 + 0] = (unsigned char) (((unsigned int) palette[best1 * 3 + 0] + (unsigned int) palette[best2 * 3 + 0]) >> 1);
						palette[best1 * 3 + 1] = (unsigned char) (((unsigned int) palette[best1 * 3 + 1] + (unsigned int) palette[best2 * 3 + 1]) >> 1);
						palette[best1 * 3 + 2] = (unsigned char) (((unsigned int) palette[best1 * 3 + 2] + (unsigned int) palette[best2 * 3 + 2]) >> 1);
						paletteuse[best1] += paletteuse[best2];
						colors--;
						palette[best2 * 3 + 0] = palette[colors * 3 + 0];
						palette[best2 * 3 + 1] = palette[colors * 3 + 1];
						palette[best2 * 3 + 2] = palette[colors * 3 + 2];
						paletteuse[best2] = paletteuse[colors];
					}
				}
				hz_bitstream_write_bits(s->frameblocks, palettebits, 3);
				for (i = 0;i < (1 << palettebits);i++)
					hz_bitstream_write_bits(s->frameblocks, (palette[i * 3 + 0] << 16) | (palette[i * 3 + 1] << 8) | palette[i * 3 + 2], 24);
				if (palettebits)
				{
					for (b = 0;b < bh;b++)
					{
						for (a = 0;a < bw;a++)
						{
							pix = pixels + ((y1 + b) * width + (x1 + a)) * 3;
							cr = pix[0];
							cg = pix[1];
							cb = pix[2];
							best = -1;
							bestscore = 1000000000;
							for (i = 0;i < colors;i++)
							{
								rdist = palette[i * 3 + 0] - cr;
								gdist = palette[i * 3 + 1] - cg;
								bdist = palette[i * 3 + 2] - cb;
								score = rdist * rdist + gdist * gdist + bdist * bdist;
								if (bestscore > score)
								{
									best = i;
									bestscore = score;
								}
							}
							error += bestscore;
							hz_bitstream_write_bits(s->frameblocks, best, palettebits);
							p = s->videopixels + ((y1 + b) * width + (x1 + a)) * 3;
							p[0] = palette[best * 3 + 0];
							p[1] = palette[best * 3 + 1];
							p[2] = palette[best * 3 + 2];
						}
					}
				}
				else
				{
					for (b = 0;b < bh;b++)
					{
						for (a = 0;a < bw;a++)
						{
							pix = pixels + ((y1 + b) * width + (x1 + a)) * 3;
							cr = pix[0];
							cg = pix[1];
							cb = pix[2];
							rdist = palette[0] - cr;
							gdist = palette[1] - cg;
							bdist = palette[2] - cb;
							bestscore = rdist * rdist + gdist * gdist + bdist * bdist;
							error += bestscore;
							p = s->videopixels + ((y1 + b) * width + (x1 + a)) * 3;
							p[0] = palette[0];
							p[1] = palette[1];
							p[2] = palette[2];
						}
					}
				}
			}
			errorblock = error - errorstart;
			if (errorbiggest < errorblock)
				errorbiggest = errorblock;
		}
	}
	error = sqrt(error) * (1.0 / 256.0);
	errorbiggest = sqrt(errorbiggest) * (1.0 / 256.0);
	printf("average degradation: %f%% most degraded block: %f%% percentage delta compressed: %f%%\n", error * 100.0 / (width * height), errorbiggest * 100.0 / (BLOCKSIZE * BLOCKSIZE), (double) deltablocks * 100.0 / (double) (s->blockswidth * s->blocksheight));
}

static int dpvencode_beginstream(dpvencodestream_t *s, unsigned int width, unsigned int height, double videoframerate, double videoquality/*, unsigned int samplespersecond, double soundquality*/)
{
	if (width < 1 || width >= 65536)
		s->error = DPVENCODEERROR_INVALIDWIDTH;
	if (height < 1 || height >= 65536)
		s->error = DPVENCODEERROR_INVALIDHEIGHT;
	if (videoframerate < 0.0001 || videoframerate > 1000.0)
		s->error = DPVENCODEERROR_INVALIDFRAMERATE;
	if (videoquality < 0.000001 || videoquality > 1)
		s->error = DPVENCODEERROR_INVALIDVIDEOQUALITY;

//	if (samplespersecond >= 65536)
//		s->error = DPVENCODEERROR_INVALIDSAMPLESPERSECOND;
//	if (soundquality < 0.000001 || soundquality > 1)
//		s->error = DPVENCODEERROR_INVALIDAUDIOQUALITY;

	if (s->error)
		return s->error;

	//dpvencode_buildsoundtable(s);

	if (s->error)
		return s->error;

	if (s->videopixels)
		free(s->videopixels);
	if (s->blockinfo)
		free(s->blockinfo);

	s->width = width;
	s->height = height;
	s->framerate = videoframerate;
	s->videoquality = videoquality;
//	s->samplespersecond = samplespersecond;
//	s->soundquality = soundquality;

	s->blockswidth = (width + BLOCKSIZE - 1) / BLOCKSIZE;
	s->blocksheight = (height + BLOCKSIZE - 1) / BLOCKSIZE;
	s->videopixels = malloc(s->width * s->height * 3);
	s->blockinfo = malloc(s->blockswidth * s->blocksheight * sizeof(*s->blockinfo));
	memset(s->videopixels, 0, s->width * s->height * 3);
	memset(s->blockinfo, 0, s->blockswidth * s->blocksheight * sizeof(*s->blockinfo));

	hz_bitstream_write_byte(s->headerblocks, 'D');
	hz_bitstream_write_byte(s->headerblocks, 'P');
	hz_bitstream_write_byte(s->headerblocks, 'V');
	hz_bitstream_write_byte(s->headerblocks, 'i');
	hz_bitstream_write_byte(s->headerblocks, 'd');
	hz_bitstream_write_byte(s->headerblocks, 'e');
	hz_bitstream_write_byte(s->headerblocks, 'o');
	hz_bitstream_write_byte(s->headerblocks, 0);

	hz_bitstream_write_short(s->headerblocks, 1); // version

	// video info goes here
	hz_bitstream_write_short(s->headerblocks, s->width);
	hz_bitstream_write_short(s->headerblocks, s->height);
	hz_bitstream_write_int(s->headerblocks, (unsigned int) (s->framerate * 65536.0));
	// sound info goes here
	// samplespersecond (no sound here, so zero)
	hz_bitstream_write_int(s->headerblocks, 0);

	hz_bitstream_write_writeblocks(s->headerblocks, s->bitstream);

	return s->error;
}

int dpvencode_video(void *stream, unsigned char *pixels)
{
	dpvencodestream_t *s;
	int oldsize;
	s = stream;
	oldsize = hz_bitstream_write_currentbyte(s->bitstream);

	if (s->error)
		return s->error;

	// video information

	// video data
	dpvencode_compressimage(s, pixels, s->videoquality);
	hz_bitstream_write_flushbits(s->frameblocks);

	// now that we know the size of the frame we can make the header
	hz_bitstream_write_bytes(s->headerblocks, "VID0", 4);
	hz_bitstream_write_int(s->headerblocks, hz_bitstream_write_sizeofblocks(s->frameblocks));
	hz_bitstream_write_writeblocks(s->headerblocks, s->bitstream);
	hz_bitstream_write_writeblocks(s->frameblocks, s->bitstream);

	printf("video frame %d size: %.3fk\n", s->videoframenum, (double) (hz_bitstream_write_currentbyte(s->bitstream) - oldsize) * (1.0 / 1024.0));
	s->videoframenum++;
	// this is 0 if there is no error
	return s->error;
}

/*
int dpvencode_audio(void *stream, short *sound, unsigned int soundlen)
{
	dpvencodestream_t *s;
	int oldsize;
	// if no audio is supplied, just skip it
	if (samplespersecond < 1 || soundlen < 1)
		return DPVENCODEERROR_NONE;
	s = stream;

	if (s->error)
		return s->error;

	oldsize = hz_bitstream_write_currentbyte(s->bitstream);

	// sound information
	hz_bitstream_write_int(s->frameblocks, soundlen);

	// sound data
	dpvencode_sound(s, sound, soundlen, quality);
	hz_bitstream_write_flushbits(s->frameblocks);

	// now that we know the size of the frame we can make the header
	hz_bitstream_write_bytes(s->headerblocks, "AUD0", 4);
	hz_bitstream_write_int(s->headerblocks, hz_bitstream_write_sizeofblocks(s->frameblocks));
	hz_bitstream_write_writeblocks(s->headerblocks, s->bitstream);
	hz_bitstream_write_writeblocks(s->frameblocks, s->bitstream);

	printf("audio frame %d size: %.3fk\n", s->audioframenum, (double) (hz_bitstream_write_currentbyte(s->bitstream) - oldsize) * (1.0 / 1024.0));
	s->audioframenum++;
	// this is 0 if there is no error
	return s->error;
}
*/

int dpvencode_error(void *stream, char **errorstring)
{
	dpvencodestream_t *s = stream;
	if (errorstring)
	{
		*errorstring = NULL;
		switch (s->error)
		{
			case DPVENCODEERROR_NONE:
				*errorstring = "no error";
				break;
			case DPVENCODEERROR_CHANGEDPARAMETERS:
				*errorstring = "video settings (width, height, framerate, etc) changed during stream";
				break;
			case DPVENCODEERROR_WRITEERROR:
				*errorstring = "write error (out of disk space?)";
				break;
			case DPVENCODEERROR_INVALIDWIDTH:
				*errorstring = "invalid image width";
				break;
			case DPVENCODEERROR_INVALIDHEIGHT:
				*errorstring = "invalid image height";
				break;
			case DPVENCODEERROR_INVALIDFRAMERATE:
				*errorstring = "invalid video framerate";
				break;
			case DPVENCODEERROR_INVALIDSAMPLESPERSECOND:
				*errorstring = "invalid audio rate (samples per second)";
				break;
			case DPVENCODEERROR_INVALIDVIDEOQUALITY:
				*errorstring = "invalid video quality";
				break;
			case DPVENCODEERROR_INVALIDAUDIOQUALITY:
				*errorstring = "invalid video quality";
				break;
			default:
				*errorstring = "unknown error";
				break;
		}
	}
	return s->error;
}

void *dpvencode_open(char *filename, char **errorstring, int width, int height, double framerate, double videoquality/*, int samplespersecond, double audioquality*/)
{
	dpvencodestream_t *s;
	if (errorstring != NULL)
		*errorstring = NULL;
	s = malloc(sizeof(*s));
	if (s != NULL)
	{
		memset(s, 0, sizeof(*s));
		s->bitstream = hz_bitstream_write_open(filename);
		if (s->bitstream != NULL)
		{
			//s->audiohuffmantree = hz_huffman_write_newtree(64);
			//if (s->audiohuffmantree != NULL)
			//{
				s->headerblocks = hz_bitstream_write_allocblocks();
				if (s->headerblocks != NULL)
				{
					s->frameblocks = hz_bitstream_write_allocblocks();
					if (s->frameblocks != NULL)
					{
						if (!dpvencode_beginstream(s, width, height, framerate, videoquality))
							return s;
						hz_bitstream_write_freeblocks(s->frameblocks);
					}
					else if (errorstring != NULL)
						*errorstring = "unable to allocate memory for frame block chain";
					hz_bitstream_write_freeblocks(s->headerblocks);
				}
				else if (errorstring != NULL)
					*errorstring = "unable to allocate memory for header block chain";
			//	hz_huffman_write_freetree(s->audiohuffmantree);
			//}
			//else if (errorstring != NULL)
			//	*errorstring = "unable to allocate memory for audio huffman tree";
			hz_bitstream_write_close(s->bitstream);
		}
		else if (errorstring != NULL)
			*errorstring = "unable to open file";
		free(s);
	}
	else if (errorstring != NULL)
		*errorstring = "unable to allocate memory for stream structure";
	return NULL;
}

void dpvencode_close(void *stream)
{
	dpvencodestream_t *s;
	s = stream;
	if (s == NULL)
		return;
	hz_bitstream_write_freeblocks(s->headerblocks);
	hz_bitstream_write_freeblocks(s->frameblocks);
	//hz_huffman_write_freetree(s->audiohuffmantree);
	hz_bitstream_write_close(s->bitstream);
	free(s);
}
