
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "dpvdecode.h"
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

void StripExtension(char *in, char *out)
{
	char *dot;
	dot = strrchr(in, '.');
	if (dot)
	{
		if (dot < strrchr(in, '/'))
			dot = NULL;
		if (dot < strrchr(in, '\\'))
			dot = NULL;
		if (dot < strrchr(in, ':'))
			dot = NULL;
	}
	if (dot == NULL)
		dot = in + strlen(in);
	while (in < dot)
		*out++ = *in++;
	*out++ = 0;
}


void usage(void)
{
	printf(
"usage: dpvdecoder <name>\n"
"example:\n"
"dpvdecoder test\n"
"would save out all the frames of test.dpv as test00000.tga, test00001.tga\n"
"and so on, and save out the sound as test.wav\n"
	);
}

int main(int argc, char **argv)
{
	int errornum;
	int width, height/*, soundbufferlength, soundlength*/, framenum;
	void *imagedata;
	//short *sounddata;
	char *errormessage;
	void *stream;
	char *basename;
	char filename[1024];
	char framename[1024];
	char wavname[1024];
	if (argc != 2)
	{
		usage();
		return 1;
	}
	basename = argv[1];

	sprintf(filename, "%s.dpv", basename);
	stream = dpvdecode_open(filename, &errormessage);
	if (stream == NULL)
	{
		printf("unable to open stream file \"%s\", file does not exist or is not a valid stream\ndpvdecode_error reported: %s\n", filename, errormessage);

		strcpy(filename, basename);
		StripExtension(basename, basename);
		stream = dpvdecode_open(filename, &errormessage);
		if (stream == NULL)
		{
			printf("unable to open stream file \"%s\", file does not exist or is not a valid stream\ndpvdecode_error reported: %s\n", filename, errormessage);
			return 1;
		}
	}

	sprintf(wavname, "%s.wav", basename);
	// FIXME: write wav save code
	errornum = 0;
	width = dpvdecode_getwidth(stream);
	height = dpvdecode_getheight(stream);
	//soundbufferlength = dpvdecode_getneededsoundbufferlength(stream);
	imagedata = malloc(width * height * 4);
	//sounddata = malloc(soundlength * sizeof(short[2]));
	for (framenum = 0;;framenum++)
	{
		//dpvdecode_frame(stream, framenum, imagedata, 0xFF0000, 0x00FF00, 0x0000FF, 4, width * 4, sounddata, soundbufferlength, &soundlength);
		dpvdecode_video(stream, framenum, imagedata, 0xFF0000, 0x00FF00, 0x0000FF, 4, width * 4);
		if (dpvdecode_error(stream, &errormessage))
			break;
		sprintf(framename, "%s%04d.tga", basename, framenum);
		savetga_rgb32_topdown(framename, imagedata, width, height);
	}
	if (errornum)
		printf("error while decoding stream: %s\n", errormessage);
	dpvdecode_close(stream);
	return 0;
}
