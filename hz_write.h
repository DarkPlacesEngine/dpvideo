
#ifndef HZLIB_BITSTREAM_WRITE_H
#define HZLIB_BITSTREAM_WRITE_H

#include <stdio.h>

#define HZWRITEERROR_OK 0
#define HZWRITEERROR_INVALIDTREE 2

#define HZWRITEBLOCKSIZE 60000

typedef struct hz_bitstream_writeblock_s
{
	struct hz_bitstream_writeblock_s *next;
	int size;
	unsigned char data[HZWRITEBLOCKSIZE];
}
hz_bitstream_writeblock_t;

typedef struct
{
	// bit storage
	unsigned int store;
	int count;
	// data storage after bits combined into bytes
	hz_bitstream_writeblock_t *blocks;
}
hz_bitstream_writeblocks_t;

typedef struct
{
	FILE *file;
	int endoffile;
}
hz_bitstream_write_t;

hz_bitstream_write_t *hz_bitstream_write_open(char *filename);
hz_bitstream_write_t *hz_bitstream_write_openfh(FILE *file);
void hz_bitstream_write_close(hz_bitstream_write_t *stream);
unsigned int hz_bitstream_write_currentbyte(hz_bitstream_write_t *stream);
int hz_bitstream_write_seek(hz_bitstream_write_t *stream, unsigned int position);


hz_bitstream_writeblocks_t *hz_bitstream_write_allocblocks(void);
void hz_bitstream_write_freeblocks(hz_bitstream_writeblocks_t *blocks);
void hz_bitstream_write_clearblocks(hz_bitstream_writeblocks_t *blocks);
void hz_bitstream_write_writeblocks(hz_bitstream_writeblocks_t *blocks, hz_bitstream_write_t *stream);
unsigned int hz_bitstream_write_sizeofblocks(hz_bitstream_writeblocks_t *blocks);

void hz_bitstream_write_byte(hz_bitstream_writeblocks_t *blocks, unsigned int num);
void hz_bitstream_write_short(hz_bitstream_writeblocks_t *blocks, unsigned int num);
void hz_bitstream_write_int(hz_bitstream_writeblocks_t *blocks, unsigned int num);
void hz_bitstream_write_bytes(hz_bitstream_writeblocks_t *blocks, void *data, unsigned int size);

void hz_bitstream_write_bit(hz_bitstream_writeblocks_t *blocks, unsigned int bit);
void hz_bitstream_write_bits(hz_bitstream_writeblocks_t *blocks, unsigned int num, unsigned int size);
void hz_bitstream_write_flushbits(hz_bitstream_writeblocks_t *blocks);




// symbols are limited to 31bit because it is possible (common even?) for a
// symbol to be 0 bits if unused, thus 0-31 is the range of possible symbol
// lengths, rather than 1-32
#define HZWRITESYMBOLLENGTHBITS 5
#define HZWRITEMAXSYMBOLBITS 31

typedef struct hzhuffmanwritenode_s
{
	// (leaf or node)
	// linked list for sorted insertion/extraction,
	// only used when building tree
	struct hzhuffmanwritenode_s *next;
	// (leaf or node)
	struct hzhuffmanwritenode_s *parent;
	// (leaf or node)
	struct hzhuffmanwritenode_s *children[2];
	// (leaf)
	// used for building trees only
	unsigned int count;
	// (leaf)
	unsigned int bits;
	// (leaf)
	unsigned int length;
	// (leaf)
	// used when writing really huge (> 32bit) codes
	unsigned int mark;
}
hzhuffmanwritenode_t;

typedef struct hzhuffmanwritetree_s
{
	unsigned int maxsymbols;
	unsigned int maxnodes;

	// internal use only
	unsigned int mark;

	// nodes and symbols (the symbols are the first nodes)
	hzhuffmanwritenode_t *nodes;

	// pointer to the root node
	hzhuffmanwritenode_t *root;
}
hzhuffmanwritetree_t;

hzhuffmanwritetree_t *hz_huffman_write_newtree(unsigned int maxsymbols);
void hz_huffman_write_freetree(hzhuffmanwritetree_t *h);
void hz_huffman_write_clearcounts(hzhuffmanwritetree_t *h);
void hz_huffman_write_countsymbol(hzhuffmanwritetree_t *h, unsigned int num);
void hz_huffman_write_uncountsymbol(hzhuffmanwritetree_t *h, unsigned int num);
void hz_huffman_write_buildtree(hzhuffmanwritetree_t *h);
void hz_huffman_write_writetree(hz_bitstream_writeblocks_t *blocks, hzhuffmanwritetree_t *h);
void hz_huffman_write_writesymbol(hz_bitstream_writeblocks_t *blocks, hzhuffmanwritetree_t *h, unsigned int num);
int hz_huffman_write_getsymbollength(hzhuffmanwritetree_t *h, unsigned int num);

#endif
