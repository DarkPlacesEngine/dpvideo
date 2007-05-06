
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dpvdecode.h"
#include "SDL.h"
#include "SDL_main.h"

#if WIN32
#include <windows.h>
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

typedef int GLint;
typedef unsigned int GLuint;
typedef int GLenum;
typedef int GLsizei;
typedef float GLfloat;
typedef double GLdouble;
typedef void GLvoid;
typedef float GLclampf;
typedef int GLbitfield;

// which GL library to load depends on platform...
/*
#if WIN32
#define GL_LIBRARY "opengl32.dll"
#else
#define GL_LIBRARY "libGL.so.1"
#endif
*/
#define GL_LIBRARY NULL

static int quit;

static void binarystring(char *s, unsigned int n, unsigned int digits)
{
	s += digits;
	*s = 0;
	while (digits--)
	{
		*(--s) = '0' + (n & 1);
		n >>= 1;
	}
}

static void printvideoinfo(const SDL_VideoInfo *videoinfo)
{
	char tempstring[48];
	printf("videoinfo:\n");
	printf("hardware available: %s\n", videoinfo->hw_available ? "yes" : "no");
	printf("window manager available: %s\n", videoinfo->wm_available ? "yes" : "no");
	printf("hardware to hardware blits accelerated: %s\n", videoinfo->blit_hw ? "yes" : "no");
	printf("hardware to hardware colorkey blits accelerated: %s\n", videoinfo->blit_hw ? "yes" : "no");
	printf("hardware to hardware alpha blits accelerated: %s\n", videoinfo->blit_hw ? "yes" : "no");
	printf("software to hardware blits accelerated: %s\n", videoinfo->blit_hw ? "yes" : "no");
	printf("software to hardware colorkey blits accelerated: %s\n", videoinfo->blit_hw ? "yes" : "no");
	printf("software to hardware alpha blits accelerated: %s\n", videoinfo->blit_hw ? "yes" : "no");
	printf("color fills accelerated: %s\n", videoinfo->blit_hw ? "yes" : "no");
	printf("video memory: %.3fmb (%ikb)\n", videoinfo->video_mem / 1024.0, videoinfo->video_mem);
	printf("videoinfo pixel format:\n");
	printf("bits per pixel: %d\n", videoinfo->vfmt->BitsPerPixel);
	printf("bytes per pixel: %d\n", videoinfo->vfmt->BytesPerPixel);
	binarystring(tempstring, videoinfo->vfmt->Rmask, 32);
	printf("Rmask: %s Rshift: %2d Rloss: %2d\n", tempstring, videoinfo->vfmt->Rshift, videoinfo->vfmt->Rloss);
	binarystring(tempstring, videoinfo->vfmt->Gmask, 32);
	printf("Gmask: %s Gshift: %2d Gloss: %2d\n", tempstring, videoinfo->vfmt->Gshift, videoinfo->vfmt->Gloss);
	binarystring(tempstring, videoinfo->vfmt->Bmask, 32);
	printf("Bmask: %s Bshift: %2d Bloss: %2d\n", tempstring, videoinfo->vfmt->Bshift, videoinfo->vfmt->Bloss);
	binarystring(tempstring, videoinfo->vfmt->Amask, 32);
	printf("Amask: %s Ashift: %2d Aloss: %2d\n", tempstring, videoinfo->vfmt->Ashift, videoinfo->vfmt->Aloss);
	printf("colorkey: %d\n", videoinfo->vfmt->colorkey);
	printf("alpha: %d\n", videoinfo->vfmt->alpha);
}

#define GL_NO_ERROR 				0x0
#define GL_INVALID_VALUE			0x0501
#define GL_INVALID_ENUM				0x0500
#define GL_INVALID_OPERATION			0x0502
#define GL_STACK_OVERFLOW			0x0503
#define GL_STACK_UNDERFLOW			0x0504
#define GL_OUT_OF_MEMORY			0x0505

void GL_PrintError(int errornumber, char *filename, int linenumber)
{
	switch(errornumber)
	{
#ifdef GL_INVALID_ENUM
	case GL_INVALID_ENUM:
		printf("GL_INVALID_ENUM at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_INVALID_VALUE
	case GL_INVALID_VALUE:
		printf("GL_INVALID_VALUE at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_INVALID_OPERATION
	case GL_INVALID_OPERATION:
		printf("GL_INVALID_OPERATION at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_STACK_OVERFLOW
	case GL_STACK_OVERFLOW:
		printf("GL_STACK_OVERFLOW at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_STACK_UNDERFLOW
	case GL_STACK_UNDERFLOW:
		printf("GL_STACK_UNDERFLOW at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_OUT_OF_MEMORY
	case GL_OUT_OF_MEMORY:
		printf("GL_OUT_OF_MEMORY at %s:%i\n", filename, linenumber);
		break;
#endif
#ifdef GL_TABLE_TOO_LARGE
    case GL_TABLE_TOO_LARGE:
		printf("GL_TABLE_TOO_LARGE at %s:%i\n", filename, linenumber);
		break;
#endif
	default:
		printf("GL UNKNOWN (%i) at %s:%i\n", errornumber, filename, linenumber);
		break;
	}
}

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

void audiocallback(void *userdata, Uint8 *stream, int len)
{
	/*
	{
		int i;
		for (i = 0;i < len / sizeof(short);i++)
			((short *)stream)[i] = rand();
	}
	*/
	// we let SDL emulate what we want if it's not available,
	// so this just assumes 16bit stereo
	audiodequeue((void *)userdata, (void *)stream, len / sizeof(short[2]));
	/*
	{
		int i, average;
		average = 0;
		for (i = 0;i < len / sizeof(short);i++)
			average += ((short *)stream)[i];
		printf("audiocallback(%p, %p, %i) average %i\n", userdata, stream, len, average / (len / sizeof(short)));
	}
	*/
}

static void dpvplayer(char *filename, void *stream, int fullscreen, int opengl, char *libglname, int glmode)
{
	char caption[1024];
	char *errorstring;
	int i;
	int errornum;
	int sflags, fsflags;
	int width, height, glwidth, glheight;
	int bitsperpixel;
	int oldtime, currenttime;
	int oldframenum, newframenum;
	//int soundbufferlength, soundlength;
	//short *soundbuffer;
	//double speed;
	int playing;
	double timedifference;
	double streamtime;
	SDL_Event event;
	SDL_Surface *surface;
	double playedtime;
	int playedframes;
	void *imagedata;

	int soundfrequency;
	audiocallbackinfo_t *audiocallbackinfo = NULL;
	SDL_AudioSpec *desiredaudiospec = NULL;
	double soundmixahead;
	double soundprefetch;
	double soundlatency;
	int soundmixaheadsamples;
	int soundprefetchsamples;
	int soundlatencysamples;
	int audioworks = 0;
	short *soundbuffer = NULL;
	int soundbufferlength;
	int firstsample;
	int audiopaused;
	int audiopause;
	int reset;

#define GL_UNSIGNED_BYTE			0x1401
#define GL_MAX_TEXTURE_SIZE			0x0D33
#define GL_TEXTURE_2D				0x0DE1
#define GL_RGB					0x1907
#define GL_RGBA					0x1908
#define GL_BGR					0x80E0
#define GL_BGRA					0x80E1
#define GL_UNSIGNED_BYTE_3_3_2			0x8032
#define GL_UNSIGNED_BYTE_2_3_3_REV		0x8362
#define GL_UNSIGNED_SHORT_5_6_5			0x8363
#define GL_UNSIGNED_SHORT_5_6_5_REV		0x8364
#define GL_UNSIGNED_SHORT_4_4_4_4		0x8033
#define GL_UNSIGNED_SHORT_4_4_4_4_REV		0x8365
#define GL_UNSIGNED_SHORT_5_5_5_1		0x8034
#define GL_UNSIGNED_SHORT_1_5_5_5_REV		0x8366
#define GL_UNSIGNED_INT_8_8_8_8			0x8035
#define GL_UNSIGNED_INT_8_8_8_8_REV		0x8367
#define GL_UNSIGNED_INT_10_10_10_2		0x8036
#define GL_UNSIGNED_INT_2_10_10_10_REV		0x8368
#define GL_QUADS				0x0007
// FIXME: try to use this format
#define GL_UNSIGNED_SHORT_5_6_5			0x8363
#define GL_COLOR_BUFFER_BIT			0x00004000
#define GL_DEPTH_BUFFER_BIT			0x00000100
#define GL_TEXTURE_MAG_FILTER			0x2800
#define GL_TEXTURE_MIN_FILTER			0x2801
#define GL_LINEAR				0x2601
#define GL_UNSIGNED_SHORT			0x1403

	GLint maxtexturesize;
	int glRmask, glGmask, glBmask, glbpp;
	GLint gluploadformat;
	GLint glinternalformat;
	GLint glpixeltype;
	void (GLAPIENTRY *glEnable)(GLenum cap);
	void (GLAPIENTRY *glDisable)(GLenum cap);
	void (GLAPIENTRY *glGetIntegerv)(GLenum pname, GLint *params);
	void (GLAPIENTRY *glOrtho)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
	void (GLAPIENTRY *glBindTexture)(GLenum target, GLuint texture);
	void (GLAPIENTRY *glTexImage2D)(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels );
	void (GLAPIENTRY *glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
	void (GLAPIENTRY *glTexCoord2f)(GLfloat s, GLfloat t);
	void (GLAPIENTRY *glVertex2f)(GLfloat x, GLfloat y);
	void (GLAPIENTRY *glBegin)(GLenum mode);
	void (GLAPIENTRY *glEnd)(void);
	GLenum (GLAPIENTRY *glGetError)(void);
	void (GLAPIENTRY *glClearColor)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
	void (GLAPIENTRY *glClear)(GLbitfield mask);
	void (GLAPIENTRY *glTexParameteri)(GLenum target, GLenum pname, GLint param);

	glRmask = 0;
	glGmask = 0;
	glBmask = 0;
	glbpp = 0;
	gluploadformat = 0;
	glinternalformat = 0;
	glpixeltype = 0;
	glTexImage2D = NULL;
	glTexSubImage2D = NULL;
	glGetIntegerv = NULL;
	glVertex2f = NULL;
	glTexCoord2f = NULL;
	glOrtho = NULL;
	glBindTexture = NULL;
	glEnable = NULL;
	glDisable = NULL;
	glBegin = NULL;
	glEnd = NULL;
	glGetError = NULL;
	glClearColor = NULL;
	glClear = NULL;
	glTexParameteri = NULL;

	imagedata = NULL;

	errornum = 0;
	quit = 0;
	width = dpvdecode_getwidth(stream);
	height = dpvdecode_getheight(stream);
	for (glwidth = 1;glwidth < width;glwidth <<= 1);
	for (glheight = 1;glheight < height;glheight <<= 1);

	printf("Initializing SDL.\n");

	/* Initialize defaults, Video and Audio */
	if ((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) == -1))
	{
		printf("Could not initialize SDL: %s.\n", SDL_GetError());
		SDL_Quit();
		exit(-1);
	}

	fsflags = 0;
	if (fullscreen)
		fsflags = SDL_FULLSCREEN|SDL_DOUBLEBUF;
	if (opengl)
	{
		if (SDL_GL_LoadLibrary (libglname))
		{
			printf("Unable to load GL library %s\n", libglname != NULL ? libglname : "");
			SDL_Quit();
			exit(1);
		}
		SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
		sflags = SDL_OPENGL|fsflags;
		surface = SDL_SetVideoMode(dpvdecode_getwidth(stream), dpvdecode_getheight(stream), 16, sflags);
		glTexImage2D = SDL_GL_GetProcAddress("glTexImage2D");
		glTexSubImage2D = SDL_GL_GetProcAddress("glTexSubImage2D");
		glGetIntegerv = SDL_GL_GetProcAddress("glGetIntegerv");
		glVertex2f = SDL_GL_GetProcAddress("glVertex2f");
		glTexCoord2f = SDL_GL_GetProcAddress("glTexCoord2f");
		glOrtho = SDL_GL_GetProcAddress("glOrtho");
		glBindTexture = SDL_GL_GetProcAddress("glBindTexture");
		glEnable = SDL_GL_GetProcAddress("glEnable");
		glDisable = SDL_GL_GetProcAddress("glDisable");
		glBegin = SDL_GL_GetProcAddress("glBegin");
		glEnd = SDL_GL_GetProcAddress("glEnd");
		glGetError = SDL_GL_GetProcAddress("glGetError");
		glClearColor = SDL_GL_GetProcAddress("glClearColor");
		glClear = SDL_GL_GetProcAddress("glClear");
		glTexParameteri = SDL_GL_GetProcAddress("glTexParameteri");

		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxtexturesize);
		if (maxtexturesize < width && maxtexturesize < height)
		{
			printf("GL driver only allows %dx%d textures, the video is %dx%d\n", maxtexturesize, maxtexturesize, width, height);
			SDL_Quit();
			exit(1);
		}

		glRmask = glGmask = glBmask = 0;
		switch (glmode)
		{
		default:
		case 1:
			glbpp = 4;
			gluploadformat = GL_RGBA;
			glinternalformat = 3;
			glpixeltype = GL_UNSIGNED_BYTE;
			((unsigned char *)&glRmask)[0] = 0xFF;
			((unsigned char *)&glGmask)[1] = 0xFF;
			((unsigned char *)&glBmask)[2] = 0xFF;
			break;
		case 2:
			glbpp = 4;
			gluploadformat = GL_BGRA;
			glinternalformat = 3;
			glpixeltype = GL_UNSIGNED_BYTE;
			((unsigned char *)&glRmask)[2] = 0xFF;
			((unsigned char *)&glGmask)[1] = 0xFF;
			((unsigned char *)&glBmask)[0] = 0xFF;
			break;
		case 3:
			glbpp = 2;
			gluploadformat = GL_RGB;
			glinternalformat = 3;
			glpixeltype = GL_UNSIGNED_SHORT_5_6_5;
			glRmask = 0xF800;
			glGmask = 0x07E0;
			glBmask = 0x001F;
			break;
		case 4:
			glbpp = 2;
			gluploadformat = GL_RGB;
			glinternalformat = 3;
			glpixeltype = GL_UNSIGNED_SHORT_5_6_5_REV;
			glRmask = 0x001F;
			glGmask = 0x07E0;
			glBmask = 0xF800;
			break;
		}

		imagedata = malloc(glwidth * glheight * glbpp);
		memset(imagedata, 0, glwidth * glheight * glbpp);
		glBindTexture(GL_TEXTURE_2D, 1);
		if ((i = glGetError())) GL_PrintError(i, __FILE__, __LINE__);
		glTexImage2D(GL_TEXTURE_2D, 0, glinternalformat, glwidth, glheight, 0, gluploadformat, glpixeltype, imagedata);
		if ((i = glGetError())) GL_PrintError(i, __FILE__, __LINE__);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glOrtho(0, 1, 1, 0, -100, 100);
		glEnable(GL_TEXTURE_2D);
		//glClearColor(0,0,0,0);
		//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}
	else
	{
		sflags = SDL_HWSURFACE|fsflags;
		bitsperpixel = SDL_VideoModeOK(dpvdecode_getwidth(stream), dpvdecode_getheight(stream), 16, sflags);
		if (bitsperpixel != 16 && bitsperpixel != 32)
		{
			printf("Failed to get hardware 16bit surface, resorting to software surface.\n");
			sflags = SDL_SWSURFACE|fsflags;
			bitsperpixel = SDL_VideoModeOK(dpvdecode_getwidth(stream), dpvdecode_getheight(stream), 16, sflags);
			if (bitsperpixel != 16 && bitsperpixel != 32)
			{
				printf("Still did not get a usable surface, exiting.\n");
				SDL_Quit();
				exit(1);
			}
		}

		surface = SDL_SetVideoMode(dpvdecode_getwidth(stream), dpvdecode_getheight(stream), bitsperpixel, sflags);
		if (surface->format->BytesPerPixel != 2 && surface->format->BytesPerPixel != 4)
		{
			printf("Did not get a usable surface from SetVideoMode, exiting.\n");
			SDL_Quit();
			exit(1);
		}

		if ((unsigned int) surface->w < dpvdecode_getwidth(stream) || (unsigned int) surface->h < dpvdecode_getheight(stream))
		{
			printf("Surface from SetVideoMode too small for video, exiting.\n");
			SDL_Quit();
			exit(1);
		}
	}

	printf("SDL initialized.\n");

	printf("using an SDL %s %dx%dx%dbpp surface.\n", surface->flags & SDL_OPENGL ? "opengl" : (surface->flags & SDL_HWSURFACE ? "hardware" : "software"), surface->w, surface->h, surface->format->BitsPerPixel);

	printvideoinfo(SDL_GetVideoInfo());

	sprintf(caption, "dpvplayer: %s", filename);
	SDL_WM_SetCaption(caption, NULL);

	//soundbufferlength = dpvdecode_getneededsoundbufferlength(stream);
	//soundlength = 0;
	//soundbuffer = malloc(soundbufferlength * sizeof(short[2]));
	//if (!soundbuffer)
	//{
	//	printf("Unable to allocate memory for sound buffer, exiting.\n");
	//	SDL_Quit();
	//	exit(1);
	//}

	soundfrequency = dpvdecode_getsoundrate(stream);

	if (soundfrequency)
	{
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
			exit(-1);
		}
		audioworks = 1;
		soundmixahead = desiredaudiospec->samples / desiredaudiospec->freq;
		soundprefetch = soundmixahead;
		soundlatency = soundmixahead * 6;
		soundmixaheadsamples = desiredaudiospec->samples;
		soundprefetchsamples = soundmixaheadsamples;
		soundlatencysamples = soundmixaheadsamples * 6;

		audiocallbackinfo->bufferlength = MAXSOUNDBUFFER;
		audiocallbackinfo->bufferpreferred = soundprefetchsamples;

		soundbufferlength = audiocallbackinfo->bufferlength;
		soundbuffer = malloc(soundbufferlength * sizeof(short[2]));
	}

	printf("video stream is %dx%dx%gfps with %dhz audio, length %g seconds (%d frames)\n", dpvdecode_getwidth(stream), dpvdecode_getheight(stream), dpvdecode_getframerate(stream), dpvdecode_getsoundrate(stream), dpvdecode_gettotaltime(stream), dpvdecode_gettotalframes(stream));
	playedtime = 0;
	playedframes = 0;
	playing = 0;
	streamtime = 0.0;
	firstsample = 0;
	audiopaused = audiopause = 1;
	oldtime = currenttime = SDL_GetTicks();
	oldframenum = newframenum = -1;
	reset = 1;
	while (!quit)
	{
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
				/*
				case SDLK_LEFT:
					if (speed >= 0)
						speed = -1;
					else
						speed = speed - 1;
					if (speed < -16)
						speed = -16;
					break;
				case SDLK_RIGHT:
					if (speed <= 0)
						speed = 1;
					else
						speed = speed + 1;
					if (speed > 16)
						speed = 16;
					break;
				case SDLK_COMMA:
					if (speed >= 0)
						speed = -0.1;
					else
						speed = speed - 0.1;
					if (speed < -16)
						speed = -16;
					break;
				case SDLK_PERIOD:
					if (speed <= 0)
						speed = 0.1;
					else
						speed = speed + 0.1;
					if (speed > 16)
						speed = 16;
					break;
				*/
				case SDLK_LEFT:
					streamtime -= 1;
					reset = 1;
					break;
				case SDLK_RIGHT:
					streamtime += 1;
					reset = 1;
					break;
				case SDLK_UP:
					streamtime += 10;
					reset = 1;
					break;
				case SDLK_DOWN:
					streamtime -= 10;
					reset = 1;
					break;
				case SDLK_q:
				case SDLK_ESCAPE:
					quit = 1;
					break;
				case SDLK_SPACE:
				/*
					if (speed)
						speed = 0;
					else
						speed = 1;
				*/
					playing = !playing;
					reset = 1;
					break;
				case SDLK_1:
					streamtime = dpvdecode_gettotaltime(stream) * 0.0;
					reset = 1;
					break;
				case SDLK_2:
					streamtime = dpvdecode_gettotaltime(stream) * 0.1;
					reset = 1;
					break;
				case SDLK_3:
					streamtime = dpvdecode_gettotaltime(stream) * 0.2;
					reset = 1;
					break;
				case SDLK_4:
					streamtime = dpvdecode_gettotaltime(stream) * 0.3;
					reset = 1;
					break;
				case SDLK_5:
					streamtime = dpvdecode_gettotaltime(stream) * 0.4;
					reset = 1;
					break;
				case SDLK_6:
					streamtime = dpvdecode_gettotaltime(stream) * 0.5;
					reset = 1;
					break;
				case SDLK_7:
					streamtime = dpvdecode_gettotaltime(stream) * 0.6;
					reset = 1;
					break;
				case SDLK_8:
					streamtime = dpvdecode_gettotaltime(stream) * 0.7;
					reset = 1;
					break;
				case SDLK_9:
					streamtime = dpvdecode_gettotaltime(stream) * 0.8;
					reset = 1;
					break;
				case SDLK_0:
					streamtime = dpvdecode_gettotaltime(stream) * 0.9;
					reset = 1;
					break;
				default:
					break;
				}
				break;
			case SDL_KEYUP:
				break;
			case SDL_QUIT:
				quit = 1;
				break;
			default:
				break;
			}
		}
		if (quit)
			break;
		oldtime = currenttime;
		currenttime = SDL_GetTicks();
		timedifference = (currenttime - oldtime) * (1.0 / 1000.0);
		if (timedifference < 0)
			timedifference = 0;
		if (playing)
			streamtime += timedifference;
		//streamtime += speed * timedifference;
		if (streamtime < 0)
		{
			streamtime = 0;
			reset = 1;
			//speed = 0;
		}
		oldframenum = newframenum;
		newframenum = dpvdecode_framefortime(stream, streamtime);
		if (newframenum >= (signed int) dpvdecode_gettotalframes(stream))
		{
			newframenum = 0;
			streamtime = 0;
			reset = 1;
		}
		if (newframenum != oldframenum)
		{
			if (opengl)
			{
				//dpvdecode_frame(stream, newframenum, imagedata, glRmask, glGmask, glBmask, glbpp, width * glbpp, soundbuffer, soundbufferlength, &soundlength);
				dpvdecode_video(stream, newframenum, imagedata, glRmask, glGmask, glBmask, glbpp, width * glbpp);
				if ((i = glGetError())) GL_PrintError(i, __FILE__, __LINE__);
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, gluploadformat, glpixeltype, imagedata);
				if ((i = glGetError())) GL_PrintError(i, __FILE__, __LINE__);
				//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glBegin(GL_QUADS);
				glTexCoord2f(0, 0);
				glVertex2f(0, 0);
				glTexCoord2f(0, (float) height / (float) glheight);
				glVertex2f(0, 1);
				glTexCoord2f((float) width / (float) glwidth, (float) height / (float) glheight);
				glVertex2f(1, 1);
				glTexCoord2f((float) width / (float) glwidth, 0);
				glVertex2f(1, 0);
				glEnd();
				if ((i = glGetError())) GL_PrintError(i, __FILE__, __LINE__);
				// note: SDL_GL_SwapBuffers does a glFinish for us
				SDL_GL_SwapBuffers();
			}
			else
			{
				SDL_LockSurface(surface);
				//dpvdecode_frame(stream, newframenum, surface->pixels, surface->format->Rmask, surface->format->Gmask, surface->format->Bmask, surface->format->BytesPerPixel, surface->pitch, soundbuffer, soundbufferlength, &soundlength);
				dpvdecode_video(stream, newframenum, surface->pixels, surface->format->Rmask, surface->format->Gmask, surface->format->Bmask, surface->format->BytesPerPixel, surface->pitch);
				SDL_UnlockSurface(surface);
				SDL_Flip(surface);
			}
			//sprintf(caption, "dpvplayer: %s %06d", filename, newframenum);
			//SDL_WM_SetCaption(caption, NULL);
			errornum = dpvdecode_error(stream, &errorstring);
			if (errornum)
			{
				if (errornum != DPVDECODEERROR_EOF)
					printf("frame %d error: %s\n", newframenum, errorstring);
				newframenum = 0;
				streamtime = 0;
				reset = 1;
				//speed = 0;
				playing = 0;
			}
			playedtime += timedifference;
			playedframes++;
			//i = SDL_GetTicks();
			//printf("%dms per frame\n", i - currenttime);
		}
		if (reset)
		{
			if (audioworks)
			{
				firstsample = (int) ((streamtime + soundlatency) * soundfrequency);
				if (audiocallbackinfo->buffercount)
				{
					SDL_LockAudio();
					audioqueueclear(audiocallbackinfo);
					SDL_UnlockAudio();
				}
			}
			reset = 0;
		}
		if (audioworks)
		{
			if (playing)
			{
				if (audiocallbackinfo->buffercount < audiocallbackinfo->bufferpreferred)
				{
					dpvdecode_audio(stream, firstsample, soundbuffer, audiocallbackinfo->bufferlength - audiocallbackinfo->buffercount);
					firstsample += audiocallbackinfo->bufferlength - audiocallbackinfo->buffercount;
					SDL_LockAudio();
					audioenqueue(audiocallbackinfo, soundbuffer, audiocallbackinfo->bufferlength - audiocallbackinfo->buffercount);
					SDL_UnlockAudio();
				}
				if (audiopause && audiocallbackinfo->buffercount >= audiocallbackinfo->bufferpreferred)
					audiopause = 0;
			}
			else
			{
				audiopause = 1;
				// reset the position too
				firstsample = (int) ((streamtime + soundlatency) * soundfrequency);
				audioqueueclear(audiocallbackinfo);
			}
			if (audiopause)
			{
				if (!audiopaused)
				{
					SDL_PauseAudio(1);
					audiopaused = 1;
				}
			}
			else
			{
				if (audiopaused)
				{
					SDL_PauseAudio(0);
					audiopaused = 0;
				}
			}
		}

		SDL_Delay(1);
		//printf("audiocallbackinfo:\n");
		//printf("bufferstart = %i\n", audiocallbackinfo->bufferstart);
		//printf("buffercount = %i\n", audiocallbackinfo->buffercount);
		//printf("bufferpreferred = %i\n", audiocallbackinfo->bufferpreferred);
		//printf("audio state: %i\n", SDL_GetAudioStatus());
	}
	SDL_PauseAudio(1);
	SDL_CloseAudio();

	printf("%d frames played in %f seconds, frames per second: %f\n", playedframes, playedtime, playedframes / playedtime);

	/*
	while (!(errornum = dpvdecode_error(stream)))
	{
		sprintf(framename, "%s%04d.tga", basename, dpvdecode_getframe(stream));
		dpv_writetga(framename, dpvdecode_getimagedata(stream), dpvdecode_getwidth(stream), dpvdecode_getheight(stream));
		dpvdecode_nextframe(stream);
	}
	if (errornum)
		printf("error while decoding stream: %s\n", dpvdecode_errorstring(errornum));
	*/

	SDL_QuitSubSystem (SDL_INIT_AUDIO);

	// close the screen surface
	SDL_QuitSubSystem (SDL_INIT_VIDEO);

	if (desiredaudiospec)
		free(desiredaudiospec);

	if (audiocallbackinfo)
		free(audiocallbackinfo);

	if (imagedata)
		free(imagedata);

	if (soundbuffer)
		free(soundbuffer);

	printf("Quiting SDL.\n");

	/* Shutdown all subsystems */
	SDL_Quit();

	printf("Quiting....\n");
}

int main(int argc, char **argv)
{
	int glmode;
	void *stream;
	char *filename;
	char *errorstring;
	if (argc != 2 && argc != 3)
	{
		printf("usage: dpvplayer <filename.dpv>\n");
		return 1;
	}
	filename = argv[1];
	stream = dpvdecode_open(filename, &errorstring);
	if (stream == NULL)
	{
		printf("unable to play stream file \"%s\", error reported: %s\n", filename, errorstring);
		return 1;
	}
	glmode = 0;
	if (argc == 3)
		glmode = atoi(argv[2]);
	// FIXME: add option to specify which GL_LIBRARY
	dpvplayer(filename, stream, 0, glmode > 0, GL_LIBRARY, glmode);
	dpvdecode_close(stream);
	return 0;
}
