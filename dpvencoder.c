
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "dpvencode.h"
#include "tgafile.h"
#include "wavefile.h"

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

int matchpattern(char *in, char *pattern)
{
	while (*pattern)
	{
		switch (*pattern)
		{
		case '?': // match any single character
			if (!*in)
				return 0; // no match
			in++;
			pattern++;
			break;
		case '*': // match anything until following string
			if (!*in)
				return 1; // match
			while (*pattern == '*')
				pattern++;
			if (*pattern == '?')
			{
				// *? (weird)
				break;
			}
			else if (*pattern)
			{
				// *text (typical)
				while (*in && *in != *pattern)
					in++;
			}
			else
			{
				// *null (* at end of pattern)
				return 1;
			}
			break;
		default:
			if (*in != *pattern)
				return 0; // no match
			in++;
			pattern++;
			break;
		}
	}
	if (*in)
		return 0; // reached end of pattern but not end of input
	return 1; // success
}

// a little chained strings system

// variable length
typedef struct stringlistsize_s
{
	struct stringlist_s *next;
} stringlistsize;

typedef struct stringlist_s
{
	struct stringlist_s *next;
	char text[1];
} stringlist;

stringlist *stringlistappend(stringlist *current, char *text)
{
	stringlist *newitem;
	newitem = malloc(strlen(text) + 1 + sizeof(stringlistsize));
	newitem->next = NULL;
	strcpy(newitem->text, text);
	if (current)
		current->next = newitem;
	return newitem;
}

void stringlistfree(stringlist *current)
{
	stringlist *next;
	while (current)
	{
		next = current->next;
		free(current);
		current = next;
	}
}

stringlist *stringlistsort(stringlist *start)
{
	int notdone;
	stringlist *previous, *temp2, *temp3, *temp4;
	stringlist *current;
	notdone = 1;
	while (notdone)
	{
		current = start;
		notdone = 0;
		previous = NULL;
		while (current && current->next)
		{
			if (strcmp(current->text, current->next->text) > 0)
			{
				// current is greater than next
				notdone = 1;
				temp2 = current->next;
				temp3 = current;
				temp4 = current->next->next;
				if (previous)
					previous->next = temp2;
				else
					start = temp2;
				temp2->next = temp3;
				temp3->next = temp4;
				break;
			}
			previous = current;
			current = current->next;
		}
	}
	return start;
}

// operating system specific code
#ifdef WIN32
#include <io.h>
stringlist *listdirectory(char *path)
{
	char pattern[4096];
	struct _finddata_t n_file;
    long hFile;
	stringlist *start, *current;
	strcpy(pattern, path);
	strcat(pattern, "\\*");
	// ask for the directory listing handle
	hFile = _findfirst(pattern, &n_file);
	if(hFile != -1)
	{
		// start a new chain with the the first name
		start = current = stringlistappend(NULL, n_file.name);
		// iterate through the directory
		while (_findnext(hFile, &n_file) == 0)
			current = stringlistappend(current, n_file.name);
		_findclose(hFile);
		// sort the list alphanumerically
		return stringlistsort(start);
	}
	else
		return NULL;
}
#else
#include <dirent.h>
stringlist *listdirectory(char *path)
{
	DIR *dir;
	struct dirent *ent;
	stringlist *start, *current;
	dir = opendir(path);
	if (!dir)
		return NULL;
	ent = readdir(dir);
	if (!ent)
	{
		closedir(dir);
		return NULL;
	}
	start = current = stringlistappend(NULL, ent->d_name);
	while((ent = readdir(dir)))
		current = stringlistappend(current, ent->d_name);
	closedir(dir);
	// sort the list alphanumerically
	return stringlistsort(start);
}
#endif

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
"usage: dpvencoder <name> <framerate> <quality>\n"
"example:\n"
"dpvencoder test 30 20\n"
"would load as many test*.tga frames as are found, named test<number>.tga or\n"
"test_<number>.tga (where <number> is almost any number of digits with or\n"
//"without leading zeros), and set them up for playback at 30 frames per second,\n"
//"and interleave the audio from the test.wav file if it exists\n"
"without leading zeros), and set them up for playback at 30 frames per second.\n"
"tip: framerate does not need to be integer, 29.97 for NTSC for example\n"
	);
}

char *formatstrings[] =
{
	"%s_%08d.tga",
	"%s_%07d.tga",
	"%s_%06d.tga",
	"%s_%05d.tga",
	"%s_%04d.tga",
	"%s_%03d.tga",
	"%s_%02d.tga",
	"%s_%d.tga",
	"%s%08d.tga",
	"%s%07d.tga",
	"%s%06d.tga",
	"%s%05d.tga",
	"%s%04d.tga",
	"%s%03d.tga",
	"%s%02d.tga",
	"%s%d.tga",
	NULL
};

int main(int argc, char **argv)
{
	void *stream;
	unsigned int errornum, framenum, done;
	unsigned int width, height, tgawarning;
	//unsigned int samples, samples2, samplespersecond, soundbuffersamples, sampleposition, soundwarning;
	//void *soundbuffer;
	//double audioquality;
	//double prefetchsound;
	//wavefile_t *wavefile;
	char *errormessage;
	char **formatstring;
	char *basename;
	char *framename;
	double framerate;
	double videoquality;
	double compression;
	tgafile_t *tgafile;
	if (argc != 4)
	{
		usage();
		return 1;
	}
	//samplespersecond = 0;
	width = 0;
	height = 0;
	basename = argv[1];
	framerate = atof(argv[2]);
	compression = atof(argv[3]);
	if (compression < 1)
		compression = 1;
	if (compression > 1000.0)
		compression = 1000.0;
	videoquality = 1.0 / compression;
	if (videoquality > 1)
		videoquality = 1;
	if (videoquality < 0.001)
		videoquality = 0.001;
	/*
	audioquality = 10.0 / compression;
	if (audioquality > 1)
		audioquality = 1;
	if (audioquality < 0.001)
		audioquality = 0.001;
	*/
	framename = malloc(strlen(basename) + 30);
	tgafile = NULL;
	for (formatstring = formatstrings;*formatstring != NULL;formatstring++)
	{
		sprintf(framename, *formatstring, basename, 0);
		tgafile = loadtga(framename);
		if (tgafile != NULL)
			break;
	}
	if (tgafile)
	{
		width = tgafile->width;
		height = tgafile->height;
		freetga(tgafile);
	}
	if (*formatstring == NULL)
	{
		printf("unable to find ");
		for (formatstring = formatstrings;*formatstring != NULL;formatstring++)
		{
			if (formatstring[1] == NULL)
				printf("or ");
			printf(*formatstring, basename, 0);
			printf(", ");
		}
		printf("exiting\n");
		free(framename);
		return 1;
	}
	printf("encoding %s at %f frames per second, %d by %d video\n", basename, framerate, tgafile->width, tgafile->height);
	/*
	soundwarning = 1;
	sprintf(framename, "%s.wav", basename);
	wavefile = waveopen(framename);
	if (wavefile)
	{
		// FIXME: write sound converter code to fix 16bit limitation
		if (wavefile->channels == 2 && wavefile->width == 2)
		{
			printf(" with sound (%d channels, %d samples per second, %dbit)\n", wavefile->channels, wavefile->samplespersecond, wavefile->width * 8);
			samplespersecond = wavefile->samplespersecond;
		}
		else
		{
			printf("sound must be 2 channels (stereo), 16bit, ignoring sound file\n");
			waveclose(wavefile);
			wavefile = NULL;
		}
	}
	if (wavefile == NULL)
	{
		printf(" with no sound (no wav file found, or failed to open)\n");
		samplespersecond = 0;
		soundwarning = 0;
	}

	// sound buffer dynamically grows as necessary
	soundbuffersamples = 0;
	soundbuffer = NULL;
	*/

	sprintf(framename, "%s.dpv", basename);
	stream = dpvencode_open(framename, &errormessage, width, height, framerate, videoquality);
	if (stream == NULL)
	{
		printf("error opening stream \"%s\" for writing\n", framename);
		printf("error: %s\n", errormessage);
		return 1;
	}
	//sampleposition = 0;
	//prefetchsound = 0;//.5;
	tgawarning = 1;
	done = 0;
	for (framenum = 0;!(errornum = dpvencode_error(stream, &errormessage)) && !done;framenum++)
	{
		done = 1;
		/*
		if (samplespersecond)
		{
			samples = (unsigned int) ((((double) (framenum + 1) / framerate) + prefetchsound) * samplespersecond) - sampleposition;
			if (soundbuffersamples < samples)
			{
				if (soundbuffer)
					free(soundbuffer);
				soundbuffersamples = samples;
				soundbuffer = malloc(samples * sizeof(short[2]));
			}
			samples2 = 0;
			if (wavefile && !wavefile->done)
			{
				samples2 = wavread(wavefile, samples);
				memcpy(soundbuffer, wavefile->data, samples2 * sizeof(short[2]));
				if (wavefile->done)
				{
					if (soundwarning)
					{
						printf("sound finished\n");
						soundwarning = 0;
					}
				}
			}
			sampleposition += samples;
			if (samples2)
			{
				dpvencode_audio(stream, soundbuffer, samples2, samplespersecond, audioquality);
				done = 0;
			}
		}
		*/

		sprintf(framename, *formatstring, basename, framenum);
		tgafile = loadtga(framename);
		if (tgafile)
		{
			if (tgafile->width == width && tgafile->height == height)
			{
				dpvencode_video(stream, tgafile->data);
				done = 0;
			}
			else if (tgawarning)
			{
				printf("frame \"%s\" has different size (image is %dx%d, video is %dx%d) and will not be stored\n", framename, tgafile->width, tgafile->height, width, height);
				tgawarning = 0;
			}
			freetga(tgafile);
			tgafile = NULL;
		}
		else if (tgawarning)
		{
			printf("unable to open frame \"%s\" - video finished\n", framename);
			tgawarning = 0;
		}
	}
	if (tgafile != NULL)
		freetga(tgafile);
	tgafile = NULL;
	if (errornum)
		printf("error writing stream: %s\n", errormessage);
	printf("%d frames encoded\n", framenum);
	//if (wavefile && !wavefile->done)
	//	printf("more sound than video, remaining sound not saved\n");
	dpvencode_close(stream);
	//waveclose(wavefile);
	//wavefile = NULL;
	//free(soundbuffer);
	free(framename);
	return 0;
}
