
#ifndef DPVDECODE_H
#define DPVDECODE_H

#define DPVDECODEERROR_NONE 0
#define DPVDECODEERROR_EOF 1
#define DPVDECODEERROR_READERROR 2
#define DPVDECODEERROR_SOUNDBUFFERTOOSMALL 3
#define DPVDECODEERROR_INVALIDRMASK 4
#define DPVDECODEERROR_INVALIDGMASK 5
#define DPVDECODEERROR_INVALIDBMASK 6
#define DPVDECODEERROR_COLORMASKSOVERLAP 7
#define DPVDECODEERROR_COLORMASKSEXCEEDBPP 8
#define DPVDECODEERROR_UNSUPPORTEDBPP 9

// opening and closing streams

// opens a stream
void *dpvdecode_open(char *filename, char **errorstring);
// closes a stream
void dpvdecode_close(void *stream);

// utilitarian functions

// returns the current error number for the stream, and resets the error
// number to DPVDECODEERROR_NONE
// if the supplied string pointer variable is not NULL, it will be set to the
// error message
int dpvdecode_error(void *stream, char **errorstring);

// retrieve frame number for given time
int dpvdecode_framefortime(void *stream, double t);

// return the total number of frames in the stream
unsigned int dpvdecode_gettotalframes(void *stream);

// return the total time of the stream
double dpvdecode_gettotaltime(void *stream);

// returns the width of the image data
unsigned int dpvdecode_getwidth(void *stream);

// returns the height of the image data
unsigned int dpvdecode_getheight(void *stream);

// returns the sound sample rate of the stream
unsigned int dpvdecode_getsoundrate(void *stream);

// returns the framerate of the stream
double dpvdecode_getframerate(void *stream);

// returns a recommended sound buffer length (in samples)
// for decoding a single frame of this stream
unsigned int dpvdecode_getneededsoundbufferlength(void *stream);

// decodes a frame, both video and audio, to the supplied buffers
// can produce many different possible errors
// (such as too little space in supplied sound buffer)
// (note: sound is 16bit stereo native-endian, left channel first)
//int dpvdecode_frame(void *stream, int framenum, void *imagedata, unsigned int Rmask, unsigned int Gmask, unsigned int Bmask, unsigned int bytesperpixel, int imagebytesperrow, short *sounddata, unsigned int soundbufferlength, unsigned int *soundlength);

// decodes a video frame to the supplied output pixels
int dpvdecode_video(void *stream, int framenum, void *imagedata, unsigned int Rmask, unsigned int Gmask, unsigned int Bmask, unsigned int bytesperpixel, int imagebytesperrow);
// reads some sound
// (note: sound is 16bit stereo native-endian, left channel first)
int dpvdecode_audio(void *stream, int firstsample, short *soundbuffer, int requestedlength);

#endif
