
#ifndef DPVENCODE_H
#define DPVENCODE_H

#define DPVENCODEERROR_NONE 0
#define DPVENCODEERROR_CHANGEDPARAMETERS 1
#define DPVENCODEERROR_WRITEERROR 2
#define DPVENCODEERROR_INVALIDWIDTH 3
#define DPVENCODEERROR_INVALIDHEIGHT 4
#define DPVENCODEERROR_INVALIDFRAMERATE 5
#define DPVENCODEERROR_INVALIDSAMPLESPERSECOND 6
#define DPVENCODEERROR_INVALIDVIDEOQUALITY 7
#define DPVENCODEERROR_INVALIDAUDIOQUALITY 8

// encodes a frame and corresponding audio
int dpvencode_video(void *stream, unsigned char *pixels);
// opens a new stream for writing, if something fails *errorstring will be
// set to a string describing the error (but only if errorstring != NULL)
void *dpvencode_open(char *filename, char **errorstring, int width, int height, double framerate, double videoquality);
// closes a stream
void dpvencode_close(void *stream);
// retrieves an error number from a stream, and sets provided error string
// pointer to a message describing the error (if it is not NULL)
int dpvencode_error(void *stream, char **errorstring);
// retrieves a string describing any error number the encoder can produce
char *dpvencode_errorstring(int errornum);

#endif