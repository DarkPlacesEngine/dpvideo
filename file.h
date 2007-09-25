
#ifndef FILE_H
#define FILE_H

int readfile(char *filename, void **mem, unsigned int *size);
int writefile(char *filename, void *mem, unsigned int size);

#endif
