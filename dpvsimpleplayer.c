
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dpvsimpledecode.h"
#include "SDL/SDL.h"
#include "SDL/SDL_main.h"

#define MAXSOUNDBUFFER 32768
typedef struct audiocallbackinfo_s
{
	// the size of this matchs bufferlength
	short buffer[MAXSOUNDBUFFER * 2];
	unsigned int bufferstart;
	unsigned int buffercount;
	unsigned int bufferlength;
	unsigned int bufferpreferred;
}
audiocallbackinfo_t;

void audiodequeue(audiocallbackinfo_t *info, short *samples, unsigned int length)
{
	unsigned int b1, b2, l;
	l = length;
	if (l > info->buffercount)
		l = info->buffercount;
	b1 = (info->bufferstart) % MAXSOUNDBUFFER;
	if (b1 + l > MAXSOUNDBUFFER)
	{
		b2 = (b1 + l) % MAXSOUNDBUFFER;
		memcpy(samples, info->buffer + b1 * 2, (MAXSOUNDBUFFER - b1) * sizeof(short[2]));
		memcpy(samples + (MAXSOUNDBUFFER - b1) * 2, info->buffer, b2 * sizeof(short[2]));
		//printf("audiodequeue: buffer wrap %i %i\n", (MAXSOUNDBUFFER - b1), b2);
	}
	else
	{
		memcpy(samples, info->buffer + b1 * 2, l * sizeof(short[2]));
		//printf("audiodequeue: normal      %i\n", l);
	}
	if (l < length)
	{
		memset(samples + l * 2, 0, (length - l) * sizeof(short[2]));
		//printf("audiodequeue: padding with %i\n", length - l);
	}
	info->bufferstart = (info->bufferstart + l) % MAXSOUNDBUFFER;
	info->buffercount -= l;
}

void audioenqueue(audiocallbackinfo_t *info, short *samples, unsigned int length)
{
	int b2, b3;
	//printf("enqueuing %i samples\n", length);
	if (info->buffercount + length > MAXSOUNDBUFFER)
		return;
	b2 = (info->bufferstart + info->buffercount) % MAXSOUNDBUFFER;
	if (b2 + length > MAXSOUNDBUFFER)
	{
		b3 = (b2 + length) % MAXSOUNDBUFFER;
		memcpy(info->buffer + b2 * 2, samples, (MAXSOUNDBUFFER - b2) * sizeof(short[2]));
		memcpy(info->buffer, samples + (MAXSOUNDBUFFER - b2) * 2, b3 * sizeof(short[2]));
	}
	else
		memcpy(info->buffer + b2 * 2, samples, length * sizeof(short[2]));
	info->buffercount += length;
}

void audioqueueclear(audiocallbackinfo_t *info)
{
	info->buffercount = 0;
	info->bufferstart = 0;
}

static void audiocallback(void *userdata, Uint8 *stream, int len)
{
	// we let SDL emulate what we want if it's not available,
	// so this just assumes 16bit stereo
	audiodequeue((void *)userdata, (void *)stream, len / sizeof(short[2]));
}

static void dpvplayer(char *filename, void *stream, int fullscreen)
{
	char caption[1024];
	char *errorstring;
	int errornum;
	int sflags, fsflags;
	int width, height;
	int bitsperpixel;
	int oldtime, currenttime;
	int currentframenum, newframenum;
	double timedifference;
	double streamtime;
	SDL_Event event;
	SDL_Surface *surface;
	double framerate;
	void *imagedata;

	int soundfrequency;
	audiocallbackinfo_t *audiocallbackinfo;
	SDL_AudioSpec *desiredaudiospec;
	double soundlatency;
	int soundlatencysamples;
	int audioworks;
	short *soundbuffer;
	int soundbufferlength;
	int audiopaused;

	imagedata = NULL;

	errornum = 0;
	width = dpvsimpledecode_getwidth(stream);
	height = dpvsimpledecode_getheight(stream);

	/* Initialize defaults, Video and Audio */
	if ((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) == -1))
	{
		fprintf(stderr, "Could not initialize SDL: %s.\n", SDL_GetError());
		SDL_Quit();
		exit(-1);
	}

	fsflags = 0;
	if (fullscreen)
		fsflags = SDL_FULLSCREEN|SDL_DOUBLEBUF;
	sflags = SDL_HWSURFACE|fsflags;
	bitsperpixel = SDL_VideoModeOK(width, height, 32, sflags);
	if (bitsperpixel != 16 && bitsperpixel != 32)
	{
		printf("Failed to get hardware 32bit or 16bit surface, resorting to software surface.\n");
		sflags = SDL_SWSURFACE|fsflags;
		bitsperpixel = SDL_VideoModeOK(width, height, 32, sflags);
		if (bitsperpixel != 16 && bitsperpixel != 32)
		{
			fprintf(stderr, "Still did not get a usable surface, exiting.\n");
			SDL_Quit();
			exit(1);
		}
	}

	surface = SDL_SetVideoMode(width, height, bitsperpixel, sflags);
	if (surface->format->BytesPerPixel != 2 && surface->format->BytesPerPixel != 4)
	{
		fprintf(stderr, "Did not get a usable surface from SetVideoMode, exiting.\n");
		SDL_Quit();
		exit(1);
	}

	if (surface->w < width || surface->h < height)
	{
		fprintf(stderr, "Surface from SetVideoMode too small for video, exiting.\n");
		SDL_Quit();
		exit(1);
	}

	printf("using an SDL %s %dx%dx%dbpp surface.\n", surface->flags & SDL_HWSURFACE ? "hardware" : "software", surface->w, surface->h, surface->format->BitsPerPixel);

	sprintf(caption, "dpvsimpleplayer: %s", filename);
	SDL_WM_SetCaption(caption, NULL);

	soundfrequency = dpvsimpledecode_getsoundrate(stream);

	audiocallbackinfo = malloc(sizeof(audiocallbackinfo_t));
	memset(audiocallbackinfo, 0, sizeof(audiocallbackinfo_t));

	desiredaudiospec = malloc(sizeof(SDL_AudioSpec));
	memset(desiredaudiospec, 0, sizeof(SDL_AudioSpec));

	desiredaudiospec->freq = soundfrequency;
	desiredaudiospec->format = AUDIO_S16SYS;
	desiredaudiospec->channels = 2;
	desiredaudiospec->samples = 1024;//16384;//1024;//16384;//2048;
	desiredaudiospec->callback = audiocallback;
	desiredaudiospec->userdata = audiocallbackinfo;

	// we want exactly what we asked for,
	// let SDL emulate it if not available...
	if (SDL_OpenAudio(desiredaudiospec, NULL) < 0)
	{
		fprintf(stderr, "Couldn't open audio: %s\n", SDL_GetError());
		audioworks = 0;
	}
	else
		audioworks = 1;

	if (audioworks)
	{
		audiocallbackinfo->bufferlength = MAXSOUNDBUFFER;
		audiocallbackinfo->bufferpreferred = desiredaudiospec->samples;
		soundbufferlength = audiocallbackinfo->bufferlength;
		soundbuffer = malloc(soundbufferlength * sizeof(short[2]));
		soundlatency = ((double) desiredaudiospec->samples / (double) desiredaudiospec->freq) * 6;
		soundlatencysamples = desiredaudiospec->samples * 6;
	}
	else
	{
		soundlatency = 0;
		soundlatencysamples = 0;
		soundbuffer = NULL;
	}

	printf("video stream is %dx%dx%gfps with %dhz audio\n", dpvsimpledecode_getwidth(stream), dpvsimpledecode_getheight(stream), dpvsimpledecode_getframerate(stream), dpvsimpledecode_getsoundrate(stream));

	audiopaused = 1;
	currentframenum = -1;
	streamtime = -soundlatency;
	framerate = dpvsimpledecode_getframerate(stream);

	oldtime = currenttime = SDL_GetTicks();
	for (;;)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
				case SDLK_q:
				case SDLK_ESCAPE:
					goto playingdone;
				default:
					break;
				}
				break;
			case SDL_KEYUP:
				break;
			case SDL_QUIT:
				goto playingdone;
			default:
				break;
			}
		}

		if (audioworks && audiocallbackinfo->buffercount < audiocallbackinfo->bufferpreferred)
		{
			dpvsimpledecode_audio(stream, soundbuffer, audiocallbackinfo->bufferlength - audiocallbackinfo->buffercount);
			SDL_LockAudio();
			audioenqueue(audiocallbackinfo, soundbuffer, audiocallbackinfo->bufferlength - audiocallbackinfo->buffercount);
			SDL_UnlockAudio();
		}

		oldtime = currenttime;
		currenttime = SDL_GetTicks();
		timedifference = (currenttime - oldtime) * (1.0 / 1000.0);
		if (timedifference < 0)
			timedifference = 0;

		newframenum = (int) (streamtime * framerate);
		if (newframenum < 0)
			newframenum = 0;
		if (currentframenum < newframenum)
		{
			SDL_LockSurface(surface);
			dpvsimpledecode_video(stream, surface->pixels, surface->format->Rmask, surface->format->Gmask, surface->format->Bmask, surface->format->BytesPerPixel, surface->pitch);
			SDL_UnlockSurface(surface);
			SDL_Flip(surface);
			errornum = dpvsimpledecode_error(stream, &errorstring);
			if (errornum)
			{
				if (errornum != DPVSIMPLEDECODEERROR_EOF)
					fprintf(stderr, "frame %d error: %s\n", newframenum, errorstring);
				goto playingdone;
			}
			currentframenum++;
			if (audiopaused)
			{
				audiopaused = 0;
				SDL_PauseAudio(0);
			}
		}

		streamtime += timedifference;

		SDL_Delay(1);
		//printf("audiocallbackinfo:\n");
		//printf("bufferstart = %i\n", audiocallbackinfo->bufferstart);
		//printf("buffercount = %i\n", audiocallbackinfo->buffercount);
		//printf("bufferpreferred = %i\n", audiocallbackinfo->bufferpreferred);
		//printf("audio state: %i\n", SDL_GetAudioStatus());
	}

	playingdone:
	if (audioworks)
	{
		SDL_PauseAudio(1);
		SDL_CloseAudio();
	}

	SDL_QuitSubSystem (SDL_INIT_AUDIO);
	SDL_QuitSubSystem (SDL_INIT_VIDEO);

	if (desiredaudiospec)
		free(desiredaudiospec);

	if (audiocallbackinfo)
		free(audiocallbackinfo);

	if (imagedata)
		free(imagedata);

	if (soundbuffer)
		free(soundbuffer);

	SDL_Quit();
}

int main(int argc, char **argv)
{
	void *stream;
	char *filename;
	char *errorstring;
	if (argc != 2)
	{
		fprintf(stderr, "usage: dpvsimpleplayer <filename.dpv>\n");
		return 1;
	}
	filename = argv[1];
	stream = dpvsimpledecode_open(filename, &errorstring);
	if (stream == NULL)
	{
		fprintf(stderr, "unable to play stream file \"%s\", error reported: %s\n", filename, errorstring);
		return 1;
	}
	dpvplayer(filename, stream, 0);
	dpvsimpledecode_close(stream);
	return 0;
}
