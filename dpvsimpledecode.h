
#ifndef DPVSIMPLEDECODE_H
#define DPVSIMPLEDECODE_H

#define DPVSIMPLEDECODEERROR_NONE 0
#define DPVSIMPLEDECODEERROR_EOF 1
#define DPVSIMPLEDECODEERROR_READERROR 2
#define DPVSIMPLEDECODEERROR_SOUNDBUFFERTOOSMALL 3
#define DPVSIMPLEDECODEERROR_INVALIDRMASK 4
#define DPVSIMPLEDECODEERROR_INVALIDGMASK 5
#define DPVSIMPLEDECODEERROR_INVALIDBMASK 6
#define DPVSIMPLEDECODEERROR_COLORMASKSOVERLAP 7
#define DPVSIMPLEDECODEERROR_COLORMASKSEXCEEDBPP 8
#define DPVSIMPLEDECODEERROR_UNSUPPORTEDBPP 9

// opening and closing streams

// opens a stream
void *dpvsimpledecode_open(char *filename, char **errorstring);
// closes a stream
void dpvsimpledecode_close(void *stream);

// utilitarian functions

// returns the current error number for the stream, and resets the error
// number to DPVDECODEERROR_NONE
// if the supplied string pointer variable is not NULL, it will be set to the
// error message
int dpvsimpledecode_error(void *stream, char **errorstring);

// returns the width of the image data
unsigned int dpvsimpledecode_getwidth(void *stream);

// returns the height of the image data
unsigned int dpvsimpledecode_getheight(void *stream);

// returns the sound sample rate of the stream
unsigned int dpvsimpledecode_getsoundrate(void *stream);

// returns the framerate of the stream
double dpvsimpledecode_getframerate(void *stream);

// decodes a video frame to the supplied output pixels
int dpvsimpledecode_video(void *stream, void *imagedata, unsigned int Rmask, unsigned int Gmask, unsigned int Bmask, unsigned int bytesperpixel, int imagebytesperrow);
// reads some sound
// (note: sound is 16bit stereo native-endian, left channel first)
int dpvsimpledecode_audio(void *stream, short *soundbuffer, int requestedlength);

#endif
