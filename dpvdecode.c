
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "dpvdecode.h"
#include "hz_read.h"
#include "wavefile.h"

#include "hz_read.c"
#include "wavefile.c"

// this many frames indexed per table
#define SEEKTABLESIZE 16000

// this is one gigabyte, the positionmajor indexes at this scale,
// positionminor is used for positioning inside a particular gigabyte
#define SEEKPOSITIONMAJORSCALE 1024*1024*1024

#define BLOCKSIZE 8

typedef struct dpvdecodeseek_s
{
	int position;
}
dpvdecodeseek_t;

typedef struct dpvdecodeseektable_s
{
	struct dpvdecodeseektable_s *next;
	unsigned int entries;
	dpvdecodeseek_t entry[SEEKTABLESIZE];
}
dpvdecodeseektable_t;

typedef struct dpvdecodestream_s
{
	hz_bitstream_read_t *bitstream;
	hz_bitstream_readblocks_t *framedatablocks;

	dpvdecodeseektable_t *seektables;

	int error;

	double info_framerate;
	unsigned int info_frames;

	unsigned int info_imagewidth;
	unsigned int info_imageheight;
	unsigned int info_imagebpp;
	unsigned int info_imageRloss;
	unsigned int info_imageRmask;
	unsigned int info_imageRshift;
	unsigned int info_imageGloss;
	unsigned int info_imageGmask;
	unsigned int info_imageGshift;
	unsigned int info_imageBloss;
	unsigned int info_imageBmask;
	unsigned int info_imageBshift;
	unsigned int info_imagesize;

	// current video frame (needed because of delta compression)
	int videoframenum;
	// current video frame data (needed because of delta compression)
	unsigned int *videopixels;

	// wav file the sound is being read from
	wavefile_t *wavefile;
}
dpvdecodestream_t;

static void dpvdecode_addframetoseektable(dpvdecodestream_t *s, FILE *file, int position)
{
	dpvdecodeseektable_t *seektable, **st;

	st = &s->seektables;
	while ((*st != NULL) && (*st)->entries == SEEKTABLESIZE)
		st = &(*st)->next;
	if (*st == NULL)
	{
		*st = malloc(sizeof(dpvdecodeseektable_t));
		memset(*st, 0, sizeof(dpvdecodeseektable_t));
	}

	seektable = *st;
	seektable->entry[seektable->entries++].position = position;
}

static dpvdecodeseek_t *dpvdecode_findseekentry(dpvdecodestream_t *s, unsigned int framenum)
{
	dpvdecodeseektable_t *seektable;

	seektable = s->seektables;
	while (seektable->next && framenum >= SEEKTABLESIZE)
	{
		seektable = seektable->next;
		framenum -= SEEKTABLESIZE;
	}
	if (framenum >= seektable->entries)
	{
		// tried to seek past end
		return NULL;
	}
	return seektable->entry + framenum;
}

static unsigned int dpvdecode_countseektables(dpvdecodestream_t *s)
{
	unsigned int count;
	dpvdecodeseektable_t *seektable;

	count = 0;
	seektable = s->seektables;
	while (seektable)
	{
		count += seektable->entries;
		seektable = seektable->next;
	}
	return count;
}

// this code only works for files < 2gig
static int dpvdecode_seektoposition(dpvdecodestream_t *s, int position)
{
	return hz_bitstream_read_seek(s->bitstream, position);
}

static int dpvdecode_realseektoframe(dpvdecodestream_t *s, int framenum)
{
	dpvdecodeseek_t *seekentry;
	if (framenum < 0)
		return 1;
	seekentry = dpvdecode_findseekentry(s, framenum);
	if (seekentry == NULL)
		return 1;
	return dpvdecode_seektoposition(s, seekentry->position);
}

static int dpvdecode_setpixelformat(dpvdecodestream_t *s, unsigned int Rmask, unsigned int Gmask, unsigned int Bmask, unsigned int bytesperpixel)
{
	int Rshift, Rbits, Gshift, Gbits, Bshift, Bbits;
	if (!Rmask)
	{
		s->error = DPVDECODEERROR_INVALIDRMASK;
		return s->error;
	}
	if (!Gmask)
	{
		s->error = DPVDECODEERROR_INVALIDGMASK;
		return s->error;
	}
	if (!Bmask)
	{
		s->error = DPVDECODEERROR_INVALIDBMASK;
		return s->error;
	}
	if (Rmask & Gmask || Rmask & Bmask || Gmask & Bmask)
	{
		s->error = DPVDECODEERROR_COLORMASKSOVERLAP;
		return s->error;
	}
	switch (bytesperpixel)
	{
	case 2:
		if ((Rmask | Gmask | Bmask) > 65536)
		{
			s->error = DPVDECODEERROR_COLORMASKSEXCEEDBPP;
			return s->error;
		}
		break;
	case 4:
		break;
	default:
		s->error = DPVDECODEERROR_UNSUPPORTEDBPP;
		return s->error;
		break;
	}
	for (Rshift = 0;!(Rmask & 1);Rshift++, Rmask >>= 1);
	for (Gshift = 0;!(Gmask & 1);Gshift++, Gmask >>= 1);
	for (Bshift = 0;!(Bmask & 1);Bshift++, Bmask >>= 1);
	if (((Rmask + 1) & Rmask) != 0)
	{
		s->error = DPVDECODEERROR_INVALIDRMASK;
		return s->error;
	}
	if (((Gmask + 1) & Gmask) != 0)
	{
		s->error = DPVDECODEERROR_INVALIDGMASK;
		return s->error;
	}
	if (((Bmask + 1) & Bmask) != 0)
	{
		s->error = DPVDECODEERROR_INVALIDBMASK;
		return s->error;
	}
	for (Rbits = 0;Rmask & 1;Rbits++, Rmask >>= 1);
	for (Gbits = 0;Gmask & 1;Gbits++, Gmask >>= 1);
	for (Bbits = 0;Bmask & 1;Bbits++, Bmask >>= 1);
	if (Rbits > 8)
	{
		Rshift += (Rbits - 8);
		Rbits = 8;
	}
	if (Gbits > 8)
	{
		Gshift += (Gbits - 8);
		Gbits = 8;
	}
	if (Bbits > 8)
	{
		Bshift += (Bbits - 8);
		Bbits = 8;
	}
	s->info_imagebpp = bytesperpixel;
	s->info_imageRloss = 16 + (8 - Rbits);
	s->info_imageGloss =  8 + (8 - Gbits);
	s->info_imageBloss =  0 + (8 - Bbits);
	s->info_imageRmask = (1 << Rbits) - 1;
	s->info_imageGmask = (1 << Gbits) - 1;
	s->info_imageBmask = (1 << Bbits) - 1;
	s->info_imageRshift = Rshift;
	s->info_imageGshift = Gshift;
	s->info_imageBshift = Bshift;
	s->info_imagesize = s->info_imagewidth * s->info_imageheight * s->info_imagebpp;
	return s->error;
}

static int dpvdecode_buildframeindex(dpvdecodestream_t *s)
{
	int position/*, l*/;
	unsigned int length;
	char t[4];
	// this assumes the current position is just after the file header
	for(;;)
	{
		position = hz_bitstream_read_currentbyte(s->bitstream);
		if (hz_bitstream_read_blocks_read(s->framedatablocks, s->bitstream, 8))
			break;
		hz_bitstream_read_bytes(s->framedatablocks, t, 4);
		if (!memcmp(t, "VID0", 4))
		{
			length = hz_bitstream_read_int(s->framedatablocks);
			if (length < s->info_imagewidth * s->info_imageheight * 4)
			{
				// probably a valid frame
				dpvdecode_addframetoseektable(s, 0, position);
				// seek to next frame using length from header
				if (hz_bitstream_read_seek(s->bitstream, position + length + 8))
					break;
			}
			else
				break;
		}
		else
			break;
	}
	s->info_frames = dpvdecode_countseektables(s);
	return s->seektables == NULL;
}

static void dpvdecode_freeframeindex(dpvdecodestream_t *s)
{
	dpvdecodeseektable_t *seektable, *n;
	seektable = s->seektables;
	s->seektables = NULL;
	while (seektable)
	{
		n = seektable->next;
		free(seektable);
		seektable = n;
	}
}

// opening and closing streams

static void StripExtension(char *in, char *out)
{
	char *dot, *c;
	dot = NULL;
	for (c = in;*c;c++)
	{
		if (*c == ':' || *c == '\\' || *c == '/')
			dot = NULL;
		if (*c == '.')
			dot = c;
	}
	if (dot == NULL)
	{
		// nothing to remove
		strcpy(out, in);
		return;
	}
	else
	{
		memcpy(out, in, dot - in);
		out[dot - in] = 0;
	}
}

// opens a stream
void *dpvdecode_open(char *filename, char **errorstring)
{
	dpvdecodestream_t *s;
	char t[8], *wavename;
	if (errorstring != NULL)
		*errorstring = NULL;
	s = malloc(sizeof(dpvdecodestream_t));
	if (s != NULL)
	{
		s->bitstream = hz_bitstream_read_open(filename);
		if (s->bitstream != NULL)
		{
			// check file identification
			s->framedatablocks = hz_bitstream_read_blocks_new();
			if (s->framedatablocks != NULL)
			{
				hz_bitstream_read_blocks_read(s->framedatablocks, s->bitstream, 8);
				hz_bitstream_read_bytes(s->framedatablocks, t, 8);
				if (!memcmp(t, "DPVideo", 8))
				{
					// check version number
					hz_bitstream_read_blocks_read(s->framedatablocks, s->bitstream, 2);
					if (hz_bitstream_read_short(s->framedatablocks) == 1)
					{
						hz_bitstream_read_blocks_read(s->framedatablocks, s->bitstream, 12);
						s->info_imagewidth = hz_bitstream_read_short(s->framedatablocks);
						s->info_imageheight = hz_bitstream_read_short(s->framedatablocks);
						s->info_framerate = (double) hz_bitstream_read_int(s->framedatablocks) * (1.0 / 65536.0);

						if (s->info_framerate > 0.0)
						{
							s->videopixels = malloc(s->info_imagewidth * s->info_imageheight * sizeof(*s->videopixels));
							if (s->videopixels != NULL)
							{
								if (!dpvdecode_buildframeindex(s))
								{
									wavename = malloc(strlen(filename) + 10);
									if (wavename)
									{
										StripExtension(filename, wavename);
										strcat(wavename, ".wav");
										s->wavefile = waveopen(wavename, NULL);
										free(wavename);
									}
									if (!dpvdecode_realseektoframe(s, 0))
									{
										// all is well...
										s->videoframenum = -10000;
										return s;
									}
									else if (errorstring != NULL)
										*errorstring = "error seeking to first frame";
									// error occurred, close down
									if (s->wavefile)
										waveclose(s->wavefile);
									dpvdecode_freeframeindex(s);
								}
								else if (errorstring != NULL)
									*errorstring = "error reading frames to build index table";
								free(s->videopixels);
							}
							else if (errorstring != NULL)
								*errorstring = "unable to allocate video image buffer";
						}
						else if (errorstring != NULL)
							*errorstring = "error in video info chunk";
					}
					else if (errorstring != NULL)
						*errorstring = "read error";
				}
				else if (errorstring != NULL)
					*errorstring = "not a dpvideo file";
 				hz_bitstream_read_blocks_free(s->framedatablocks);
			}
			else if (errorstring != NULL)
				*errorstring = "unable to allocate memory for reading buffer";
			hz_bitstream_read_close(s->bitstream);
		}
		else if (errorstring != NULL)
			*errorstring = "unable to open file";
		free(s);
	}
	else if (errorstring != NULL)
		*errorstring = "unable to allocate memory for stream info structure";
	return NULL;
}

// closes a stream
void dpvdecode_close(void *stream)
{
	dpvdecodestream_t *s = stream;
	if (s == NULL)
		return;
	dpvdecode_freeframeindex(s);
	if (s->videopixels)
		free(s->videopixels);
	if (s->wavefile)
		waveclose(s->wavefile);
	if (s->framedatablocks)
		hz_bitstream_read_blocks_free(s->framedatablocks);
	if (s->bitstream)
		hz_bitstream_read_close(s->bitstream);
	free(s);
}

// utilitarian functions

// returns the current error number for the stream, and resets the error
// number to DPVDECODEERROR_NONE
// if the supplied string pointer variable is not NULL, it will be set to the
// error message
int dpvdecode_error(void *stream, char **errorstring)
{
	dpvdecodestream_t *s = stream;
	int e;
	e = s->error;
	s->error = 0;
	if (errorstring)
	{
		switch (e)
		{
			case DPVDECODEERROR_NONE:
				*errorstring = "no error";
				break;
			case DPVDECODEERROR_EOF:
				*errorstring = "end of file reached (this is not an error)";
				break;
			case DPVDECODEERROR_READERROR:
				*errorstring = "read error (corrupt or incomplete file)";
				break;
			case DPVDECODEERROR_SOUNDBUFFERTOOSMALL:
				*errorstring = "sound buffer is too small for decoding frame (please allocate it as large as dpvdecode_getneededsoundbufferlength suggests)";
				break;
			case DPVDECODEERROR_INVALIDRMASK:
				*errorstring = "invalid red bits mask";
				break;
			case DPVDECODEERROR_INVALIDGMASK:
				*errorstring = "invalid green bits mask";
				break;
			case DPVDECODEERROR_INVALIDBMASK:
				*errorstring = "invalid blue bits mask";
				break;
			case DPVDECODEERROR_COLORMASKSOVERLAP:
				*errorstring = "color bit masks overlap";
				break;
			case DPVDECODEERROR_COLORMASKSEXCEEDBPP:
				*errorstring = "color masks too big for specified bytes per pixel";
				break;
			case DPVDECODEERROR_UNSUPPORTEDBPP:
				*errorstring = "unsupported bytes per pixel (must be 2 for 16bit, or 4 for 32bit)";
				break;
			default:
				*errorstring = "unknown error";
				break;
		}
	}
	return e;
}

// retrieve frame number for given time
int dpvdecode_framefortime(void *stream, double t)
{
	dpvdecodestream_t *s = stream;
	return (int) (t * s->info_framerate);
}

// return the total number of frames in the stream
unsigned int dpvdecode_gettotalframes(void *stream)
{
	dpvdecodestream_t *s = stream;
	return s->info_frames;
}

// return the total time of the stream
double dpvdecode_gettotaltime(void *stream)
{
	dpvdecodestream_t *s = stream;
	return (double) s->info_frames / s->info_framerate;
}

// returns the width of the image data
unsigned int dpvdecode_getwidth(void *stream)
{
	dpvdecodestream_t *s = stream;
	return s->info_imagewidth;
}

// returns the height of the image data
unsigned int dpvdecode_getheight(void *stream)
{
	dpvdecodestream_t *s = stream;
	return s->info_imageheight;
}

// returns the sound sample rate of the stream
unsigned int dpvdecode_getsoundrate(void *stream)
{
	dpvdecodestream_t *s = stream;
	if (s->wavefile)
		return s->wavefile->info_rate;
	else
		return 0;
}

// returns the framerate of the stream
double dpvdecode_getframerate(void *stream)
{
	dpvdecodestream_t *s = stream;
	return s->info_framerate;
}





static int dpvdecode_convertpixels(dpvdecodestream_t *s, void *imagedata, int imagebytesperrow)
{
	unsigned int a, x, y, width, height;
	unsigned int Rloss, Rmask, Rshift, Gloss, Gmask, Gshift, Bloss, Bmask, Bshift;
	unsigned int *in;

	width = s->info_imagewidth;
	height = s->info_imageheight;

	Rloss = s->info_imageRloss;
	Rmask = s->info_imageRmask;
	Rshift = s->info_imageRshift;
	Gloss = s->info_imageGloss;
	Gmask = s->info_imageGmask;
	Gshift = s->info_imageGshift;
	Bloss = s->info_imageBloss;
	Bmask = s->info_imageBmask;
	Bshift = s->info_imageBshift;

	in = s->videopixels;
	if (s->info_imagebpp == 4)
	{
		unsigned int *outrow;
		for (y = 0;y < height;y++)
		{
			outrow = (void *)((unsigned char *)imagedata + y * imagebytesperrow);
			for (x = 0;x < width;x++)
			{
				a = *in++;
				outrow[x] = (((a >> Rloss) & Rmask) << Rshift) | (((a >> Gloss) & Gmask) << Gshift) | (((a >> Bloss) & Bmask) << Bshift);
			}
		}
	}
	else
	{
		unsigned short *outrow;
		for (y = 0;y < height;y++)
		{
			outrow = (void *)((unsigned char *)imagedata + y * imagebytesperrow);
			if (Rloss == 19 && Gloss == 10 && Bloss == 3 && Rshift == 11 && Gshift == 5 && Bshift == 0)
			{
				// optimized
				for (x = 0;x < width;x++)
				{
					a = *in++;
					outrow[x] = ((a >> 8) & 0xF800) | ((a >> 5) & 0x07E0) | ((a >> 3) & 0x001F);
				}
			}
			else
			{
				for (x = 0;x < width;x++)
				{
					a = *in++;
					outrow[x] = (((a >> Rloss) & Rmask) << Rshift) | (((a >> Gloss) & Gmask) << Gshift) | (((a >> Bloss) & Bmask) << Bshift);
				}
			}
		}
	}
	return s->error;
}

static int dpvdecode_decompressimage(dpvdecodestream_t *s)
{
	int i, a, b, colors, g, x1, y1, bw, bh, width, height, palettebits;
	unsigned int palette[256], *outrow, *out;
	g = BLOCKSIZE;
	width = s->info_imagewidth;
	height = s->info_imageheight;
	for (y1 = 0;y1 < height;y1 += g)
	{
		outrow = s->videopixels + y1 * width;
		bh = g;
		if (y1 + bh > height)
			bh = height - y1;
		for (x1 = 0;x1 < width;x1 += g)
		{
			out = outrow + x1;
			bw = g;
			if (x1 + bw > width)
				bw = width - x1;
			if (hz_bitstream_read_bit(s->framedatablocks))
			{
				// updated block
				palettebits = hz_bitstream_read_bits(s->framedatablocks, 3);
				colors = 1 << palettebits;
				for (i = 0;i < colors;i++)
					palette[i] = hz_bitstream_read_bits(s->framedatablocks, 24);
				if (palettebits)
				{
					for (b = 0;b < bh;b++, out += width)
						for (a = 0;a < bw;a++)
							out[a] = palette[hz_bitstream_read_bits(s->framedatablocks, palettebits)];
				}
				else
				{
					for (b = 0;b < bh;b++, out += width)
						for (a = 0;a < bw;a++)
							out[a] = palette[0];
				}
			}
		}
	}
	return s->error;
}

// decompress a video frame by number
static int dpvdecode_decodevideoframe(dpvdecodestream_t *s, int framenum)
{
	unsigned int framedatasize;
	char t[4];
	if (dpvdecode_realseektoframe(s, framenum))
		return s->error;
	s->error = DPVDECODEERROR_NONE;
	hz_bitstream_read_blocks_read(s->framedatablocks, s->bitstream, 8);
	hz_bitstream_read_bytes(s->framedatablocks, t, 4);
	if (memcmp(t, "VID0", 4))
	{
		s->error = DPVDECODEERROR_READERROR;
		return s->error;
	}
	framedatasize = hz_bitstream_read_int(s->framedatablocks);
	hz_bitstream_read_blocks_read(s->framedatablocks, s->bitstream, framedatasize);
	return dpvdecode_decompressimage(s);
}

// decodes a video frame to the supplied output pixels
int dpvdecode_video(void *stream, int framenum, void *imagedata, unsigned int Rmask, unsigned int Gmask, unsigned int Bmask, unsigned int bytesperpixel, int imagebytesperrow)
{
	dpvdecodestream_t *s = stream;
	s->error = DPVDECODEERROR_NONE;
	if (dpvdecode_setpixelformat(s, Rmask, Gmask, Bmask, bytesperpixel))
		return s->error;
	if (framenum < 0 || framenum >= (signed int) s->info_frames)
	{
		s->videoframenum = -10000;
		memset(s->videopixels, 0, s->info_imagewidth * s->info_imageheight * sizeof(*s->videopixels));
	}
	else
	{
		if (framenum < s->videoframenum || framenum - s->videoframenum > 30)
		{
			s->videoframenum = framenum - 30;
			if (s->videoframenum < -1)
				s->videoframenum = -1;
			memset(s->videopixels, 0, s->info_imagewidth * s->info_imageheight * sizeof(*s->videopixels));
		}
		while (s->videoframenum < framenum)
		{
			s->videoframenum++;
			if (dpvdecode_decodevideoframe(s, s->videoframenum))
				return s->error;
		}
	}
	dpvdecode_convertpixels(s, imagedata, imagebytesperrow);
	return s->error;
}

// (note: sound is 16bit stereo native-endian, left channel first)
int dpvdecode_audio(void *stream, int firstsample, short *soundbuffer, int requestedlength)
{
	int sample1, sample2, startsamples, samples;
	dpvdecodestream_t *s = stream;
	s->error = DPVDECODEERROR_NONE;

	sample1 = firstsample;
	if (sample1 < 0)
		sample1 = 0;
	sample2 = firstsample + requestedlength;
	if (sample2 < 0)
		sample2 = 0;
	startsamples = sample1 - firstsample;
	if (startsamples > requestedlength)
		startsamples = requestedlength;
	if (startsamples < 0)
		startsamples = 0;
	if (startsamples > 0)
		memset(soundbuffer, 0, startsamples * sizeof(short[2]));
	samples = 0;
	if (s->wavefile && sample2 > sample1)
	{
		waveseek(s->wavefile, sample1);
		samples = waveread16stereo(s->wavefile, soundbuffer, sample2 - sample1);
	}
	startsamples += samples;
	if (startsamples < requestedlength)
		memset(soundbuffer + startsamples * 2, 0, (requestedlength - startsamples) * sizeof(short[2]));
	return s->error;
}
