
#include <stdlib.h>
#include "hz_read.h"

hz_bitstream_read_t *hz_bitstream_read_open(char *filename)
{
	FILE *file;
	hz_bitstream_read_t *stream;
	if ((file = fopen(filename, "rb")))
	{
		stream = malloc(sizeof(hz_bitstream_read_t));
		memset(stream, 0, sizeof(*stream));
		stream->file = file;
		return stream;
	}
	else
		return NULL;
}

hz_bitstream_read_t *hz_bitstream_read_openfh(FILE *file)
{
	hz_bitstream_read_t *stream;

	stream = malloc(sizeof(hz_bitstream_read_t));
	memset(stream, 0, sizeof(*stream));
	stream->file = file;
	return stream;
}

void hz_bitstream_read_close(hz_bitstream_read_t *stream)
{
	if (stream)
	{
		fclose(stream->file);
		free(stream);
	}
}

unsigned int hz_bitstream_read_currentbyte(hz_bitstream_read_t *stream)
{
	return ftell(stream->file);
}

int hz_bitstream_read_endoffile(hz_bitstream_read_t *stream)
{
	return stream->endoffile;
}

int hz_bitstream_read_seek(hz_bitstream_read_t *stream, unsigned int position)
{
	stream->endoffile = 0;
	return fseek(stream->file, position, SEEK_SET) != 0;
}

hz_bitstream_readblocks_t *hz_bitstream_read_blocks_new(void)
{
	hz_bitstream_readblocks_t *blocks;
	blocks = malloc(sizeof(hz_bitstream_readblocks_t));
	if (blocks == NULL)
		return NULL;
	memset(blocks, 0, sizeof(hz_bitstream_readblocks_t));
	return blocks;
}

void hz_bitstream_read_blocks_free(hz_bitstream_readblocks_t *blocks)
{
	hz_bitstream_readblock_t *b, *n;
	if (blocks == NULL)
		return;
	for (b = blocks->blocks;b;b = n)
	{
		n = b->next;
		free(b);
	}
	free(blocks);
}

int hz_bitstream_read_blocks_read(hz_bitstream_readblocks_t *blocks, hz_bitstream_read_t *stream, unsigned int size)
{
	int s;
	hz_bitstream_readblock_t *b, *p;
	s = size;
	p = NULL;
	b = blocks->blocks;
	while (s > 0)
	{
		if (b == NULL)
		{
			b = malloc(sizeof(hz_bitstream_readblock_t));
			if (b == NULL)
				return HZREADERROR_MALLOCFAILED;
			b->next = NULL;
			b->size = 0;
			if (p != NULL)
				p->next = b;
			else
				blocks->blocks = b;
		}
		if (s > HZREADBLOCKSIZE)
			b->size = HZREADBLOCKSIZE;
		else
			b->size = s;
		s -= b->size;
		if (fread(b->data, 1, b->size, stream->file) != b->size)
		{
			stream->endoffile = 1;
			break;
		}
		p = b;
		b = b->next;
	}
	while (b)
	{
		b->size = 0;
		b = b->next;
	}
	blocks->current = blocks->blocks;
	blocks->position = 0;
	hz_bitstream_read_flushbits(blocks);
	if (stream->endoffile)
		return HZREADERROR_EOF;
	return HZREADERROR_OK;
}

unsigned int hz_bitstream_read_blocks_getbyte(hz_bitstream_readblocks_t *blocks)
{
	while (blocks->current != NULL && blocks->position >= blocks->current->size)
	{
		blocks->position = 0;
		blocks->current = blocks->current->next;
	}
	if (blocks->current == NULL)
		return 0;
	return blocks->current->data[blocks->position++];
}

void hz_bitstream_read_flushbits(hz_bitstream_readblocks_t *blocks)
{
	blocks->store = 0;
	blocks->ungetbitstore = 0;
	blocks->count = 0;
	blocks->ungetbitcount = 0;
}

int hz_bitstream_read_bit(hz_bitstream_readblocks_t *blocks)
{
	if (!blocks->count)
	{
		blocks->count += 8;
		blocks->store <<= 8;
		blocks->store |= hz_bitstream_read_blocks_getbyte(blocks) & 0xFF;
	}
	blocks->count--;
	return (blocks->store >> blocks->count) & 1;
}

inline unsigned int hz_bitstream_read_bits(hz_bitstream_readblocks_t *blocks, unsigned int size)
{
	unsigned int num = 0;
	int b;
	// we can only handle about 24 bits at a time safely
	// (there might be up to 7 bits more than we need in the bit store)
	if (size > 24)
	{
		size -= 8;
		num |= hz_bitstream_read_bits(blocks, 8) << size;
	}
	if (blocks->ungetbitcount)
	{
		b = size;
		if (b > blocks->ungetbitcount)
			b = blocks->ungetbitcount;
		size -= b;
		blocks->ungetbitcount -= b;
		num |= ((blocks->ungetbitstore >> blocks->ungetbitcount) & ((1 << b) - 1)) << size;
	}
	while (blocks->count < size)
	{
		blocks->count += 8;
		blocks->store <<= 8;
		blocks->store |= hz_bitstream_read_blocks_getbyte(blocks) & 0xFF;
	}
	blocks->count -= size;
	num |= (blocks->store >> blocks->count) & ((1 << size) - 1);
	return num;
}

void hz_bitstream_read_ungetbits(hz_bitstream_readblocks_t *blocks, unsigned int num, unsigned int size)
{
	if (blocks->ungetbitcount + size > 32)
		// error!
		return;
	blocks->ungetbitcount += size;
	blocks->ungetbitstore <<= size;
	blocks->ungetbitstore |= num & ((1 << size) - 1);
}

unsigned int hz_bitstream_read_byte(hz_bitstream_readblocks_t *blocks)
{
	return hz_bitstream_read_blocks_getbyte(blocks);
}

unsigned int hz_bitstream_read_short(hz_bitstream_readblocks_t *blocks)
{
	return (hz_bitstream_read_byte(blocks) << 8)
	     | (hz_bitstream_read_byte(blocks));
}

unsigned int hz_bitstream_read_int(hz_bitstream_readblocks_t *blocks)
{
	return (hz_bitstream_read_byte(blocks) << 24)
	     | (hz_bitstream_read_byte(blocks) << 16)
	     | (hz_bitstream_read_byte(blocks) << 8)
	     | (hz_bitstream_read_byte(blocks));
}

void hz_bitstream_read_bytes(hz_bitstream_readblocks_t *blocks, void *outdata, unsigned int size)
{
	unsigned char *out;
	out = outdata;
	while (size--)
		*out++ = hz_bitstream_read_byte(blocks);
}













hzhuffmanreadtree_t *hz_huffman_read_newtree(unsigned int maxsymbols)
{
	hzhuffmanreadtree_t *h;
	int maxnodes, size;
	maxnodes = maxsymbols * 2 - 1;
	size = sizeof(hzhuffmanreadtree_t) + sizeof(hzhuffmanreadnode_t) * maxnodes;
	h = malloc(size);
	memset(h, 0, size);
	h->maxsymbols = maxsymbols;
	h->maxnodes = maxnodes;
	h->mark = 1;
	h->nodes = (hzhuffmanreadnode_t *)(h + 1);
	h->root = NULL;
	return h;
}

void hz_huffman_read_freetree(hzhuffmanreadtree_t *h)
{
	if (h)
	{
		// nodes was allocated as part of the struct so we don't need to free it
		free(h);
	}
}

static int hz_huffman_read_buildtreefromlengths(hzhuffmanreadtree_t *h)
{
	int i, bit, bits, mask;
	unsigned int start[HZREADMAXSYMBOLBITS + 1], lengths[HZREADMAXSYMBOLBITS + 1];
	hzhuffmanreadnode_t *node, *freenode, *endofsymbols, *p;

	// to allow iterating through the node array without a counter
	endofsymbols = freenode = h->nodes + h->maxsymbols;

	if (h->maxsymbols > 1)
	{
		// this code computes the bit strings given simply a set of symbol lengths
		memset(lengths, 0, sizeof(lengths));
		for (node = h->nodes;node < endofsymbols;node++)
			lengths[node->length]++;

		// generate the bit strings
		start[0] = start[1] = 0;
		for (i = 1;i < HZREADMAXSYMBOLBITS;i++)
			start[i + 1] = (start[i] + lengths[i]) << 1;

		bits = 0;
		for (i = HZREADMAXSYMBOLBITS;i >= 1;i--)
		{
			if ((lengths[i] + bits) & 1)
				return HZREADERROR_INVALIDTREE;
			bits = (lengths[i] + bits) >> 1;
		}

		for (node = h->nodes;node < endofsymbols;node++)
			node->bits = start[node->length]++;

		if (start[HZREADMAXSYMBOLBITS] & (((unsigned int) 1 << HZREADMAXSYMBOLBITS) - 1))
			return HZREADERROR_INVALIDTREE;

		// reconstruct the tree from the bit strings, to ensure consistent and
		// obvious tree behavior (so other implementations don't have to look at
		// the code to mimic it correctly)
		freenode = endofsymbols;
		h->root = freenode++;
		h->root->children[0] = h->root->children[1] = h->root->parent = NULL;
		for (node = h->nodes;node < endofsymbols;node++)
		{
			if (node->length)
			{
				mask = 1 << (node->length - 1);
				p = h->root;
				do
				{
					i = (node->bits & mask) != 0;
					if (p->children[i] == NULL)
					{
						if (mask > 1)
						{
							freenode->parent = p;
							freenode->children[0] = freenode->children[1] = NULL;
							p->children[i] = freenode++;
						}
						else
						{
							p->children[i] = node;
							p->children[i]->parent = p;
						}
					}
					p = p->children[i];
					mask >>= 1;
				}
				while (mask);
			}
		}
	}
	else
	{
		h->root = h->nodes;
		h->nodes->length = 0;
	}

	// build quick lookup table of bit patterns to decode the first
	// HZREADHASHSIZE bits of a symbol without having to recurse the tree
	// (if the symbol is longer than HZREADHASHSIZE, this still accelerates
	// the decoding, but only rare symbols would usually be that long, so it
	// works out nicely)
	for (i = 0;i < (1 << HZREADHASHSIZE);i++)
	{
		bits = i;
		bit = 1 << (HZREADHASHSIZE - 1);
		node = h->root;
		while (node >= endofsymbols && bit)
		{
			node = node->children[(bits & bit) != 0];
			bit >>= 1;
		}
		h->patterns[i] = node;
	}

	return HZREADERROR_OK;
}

int hz_huffman_read_treefromlengths(hzhuffmanreadtree_t *h, int *lengths)
{
	int i;

	for (i = 0;i < h->maxsymbols;)
		h->nodes[i].length = lengths[i];

	return hz_huffman_read_buildtreefromlengths(h);
}

int hz_huffman_read_readtree(hz_bitstream_readblocks_t *blocks, hzhuffmanreadtree_t *h)
{
	int i, l, bits;

	bits = 0;
	if (h->maxsymbols > 1)
	{
		for (i = 0;i < h->maxsymbols;)
		{
			l = h->nodes[i].length = hz_bitstream_read_bits(blocks, HZREADSYMBOLLENGTHBITS);
			bits += HZREADSYMBOLLENGTHBITS;
#if 0
			i++;
#else
			while ((++i) < h->maxsymbols && (bits++, hz_bitstream_read_bit(blocks) == 0))
				h->nodes[i].length = l;
#endif
		}
	}

	return hz_huffman_read_buildtreefromlengths(h);
}

unsigned int hz_huffman_read_readsymbol(hz_bitstream_readblocks_t *blocks, hzhuffmanreadtree_t *h)
{
#if 1
	hzhuffmanreadnode_t *node, *endofsymbols;
	unsigned int bits;
	endofsymbols = h->nodes + h->maxsymbols;
	// quick table lookup of bit pattern
	bits = hz_bitstream_read_bits(blocks, HZREADHASHSIZE);
	node = h->patterns[bits];
	// if the pattern did not point to a symbol, keep reading
	while (node >= endofsymbols)
		node = node->children[hz_bitstream_read_bit(blocks)];
	// if we grabbed too much, put back the extra
	if (node->length < HZREADHASHSIZE)
		hz_bitstream_read_ungetbits(blocks, bits, HZREADHASHSIZE - node->length);
	return node - h->nodes;
#else
	hzhuffmanreadnode_t *node, *endofsymbols;
	endofsymbols = h->nodes + h->maxsymbols;
	// slow-ish bit by bit parsing
	node = h->root;
	while (node >= endofsymbols)
		node = node->children[hz_bitstream_read_bit(blocks)];
	return node - h->nodes;
#endif
}
