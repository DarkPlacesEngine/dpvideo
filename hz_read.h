
#ifndef HZLIB_BITSTREAM_READ_H
#define HZLIB_BITSTREAM_READ_H

#include <stdio.h>

#define HZREADERROR_OK 0
#define HZREADERROR_EOF 1
#define HZREADERROR_MALLOCFAILED 2
#define HZREADERROR_INVALIDTREE 3

#define HZREADBLOCKSIZE 16000

typedef struct
{
	FILE *file;
	int endoffile;
}
hz_bitstream_read_t;

typedef struct hz_bitstream_readblock_s
{
	struct hz_bitstream_readblock_s *next;
	unsigned int size;
	unsigned char data[HZREADBLOCKSIZE];
}
hz_bitstream_readblock_t;

typedef struct
{
	hz_bitstream_readblock_t *blocks;
	hz_bitstream_readblock_t *current;
	unsigned int position;

	unsigned int store;
	unsigned int ungetbitstore;
	unsigned int count;
	unsigned int ungetbitcount;
}
hz_bitstream_readblocks_t;

hz_bitstream_read_t *hz_bitstream_read_open(char *filename);
hz_bitstream_read_t *hz_bitstream_read_openfh(FILE *file);
void hz_bitstream_read_close(hz_bitstream_read_t *stream);

unsigned int hz_bitstream_read_currentbyte(hz_bitstream_read_t *stream);
int hz_bitstream_read_endoffile(hz_bitstream_read_t *stream);
int hz_bitstream_read_seek(hz_bitstream_read_t *stream, unsigned int position);

hz_bitstream_readblocks_t *hz_bitstream_read_blocks_new(void);
void hz_bitstream_read_blocks_free(hz_bitstream_readblocks_t *blocks);
int hz_bitstream_read_blocks_read(hz_bitstream_readblocks_t *blocks, hz_bitstream_read_t *stream, unsigned int size);
unsigned int hz_bitstream_read_blocks_getbyte(hz_bitstream_readblocks_t *blocks);

void hz_bitstream_read_flushbits(hz_bitstream_readblocks_t *blocks);
int hz_bitstream_read_bit(hz_bitstream_readblocks_t *blocks);
unsigned int hz_bitstream_read_bits(hz_bitstream_readblocks_t *blocks, unsigned int size);
void hz_bitstream_read_ungetbits(hz_bitstream_readblocks_t *blocks, unsigned int num, unsigned int size);
unsigned int hz_bitstream_read_byte(hz_bitstream_readblocks_t *blocks);
unsigned int hz_bitstream_read_short(hz_bitstream_readblocks_t *blocks);
unsigned int hz_bitstream_read_int(hz_bitstream_readblocks_t *blocks);
void hz_bitstream_read_bytes(hz_bitstream_readblocks_t *blocks, void *outdata, unsigned int size);



// symbols are limited to 31bit because it is possible (common even?) for a
// symbol to be 0 bits if unused, thus 0-31 is the range of possible symbol
// lengths, rather than 1-32
#define HZREADSYMBOLLENGTHBITS 5
#define HZREADMAXSYMBOLBITS 31

typedef struct hzhuffmanreadnode_s
{
	// (leaf or node)
	// linked list for sorted insertion/extraction,
	// only used when building tree
	struct hzhuffmanreadnode_s *next;
	// (leaf or node)
	struct hzhuffmanreadnode_s *parent;
	// (leaf or node)
	struct hzhuffmanreadnode_s *children[2];
	// (leaf when encoding only)
	unsigned int count;
	// (leaf)
	unsigned int bits;
	// (leaf)
	unsigned int length;
	// (leaf)
	// used when writing really huge (> 32bit) codes
	unsigned int mark;
}
hzhuffmanreadnode_t;

// how big a hash index to use for faster decoding, if this is set too big it
// will run slower because of running out of CPU cache, too small and it
// wastes time reading bits individually
#define HZREADHASHSIZE 12

typedef struct hzhuffmanreadtree_s
{
	int maxsymbols;
	int maxnodes;

	// internal use only
	int mark;

	// nodes and symbols (the symbols are the first nodes)
	hzhuffmanreadnode_t *nodes;

	// pointer to the root node
	hzhuffmanreadnode_t *root;

	// quick lookup table of nodes for reading
	hzhuffmanreadnode_t *patterns[1 << HZREADHASHSIZE];
}
hzhuffmanreadtree_t;

hzhuffmanreadtree_t *hz_huffman_read_newtree(unsigned int maxsymbols);
void hz_huffman_read_freetree(hzhuffmanreadtree_t *h);
int hz_huffman_read_treefromlengths(hzhuffmanreadtree_t *h, int *lengths);
int hz_huffman_read_readtree(hz_bitstream_readblocks_t *blocks, hzhuffmanreadtree_t *h);
unsigned int hz_huffman_read_readsymbol(hz_bitstream_readblocks_t *blocks, hzhuffmanreadtree_t *h);

#endif
