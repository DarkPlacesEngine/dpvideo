
#include <stdlib.h>
#include "hz_write.h"

hz_bitstream_write_t *hz_bitstream_write_open(char *filename)
{
	FILE *file;
	hz_bitstream_write_t *stream;
	if ((file = fopen(filename, "wb")))
	{
		stream = malloc(sizeof(hz_bitstream_write_t));
		memset(stream, 0, sizeof(*stream));
		stream->file = file;
		return stream;
	}
	else
		return NULL;
}

hz_bitstream_write_t *hz_bitstream_write_openfh(FILE *file)
{
	hz_bitstream_write_t *stream;

	stream = malloc(sizeof(hz_bitstream_write_t));
	memset(stream, 0, sizeof(*stream));
	stream->file = file;
	return stream;
}

void hz_bitstream_write_close(hz_bitstream_write_t *stream)
{
	if (stream)
	{
		fclose(stream->file);
		free(stream);
	}
}

unsigned int hz_bitstream_write_currentbyte(hz_bitstream_write_t *stream)
{
	return ftell(stream->file);
}

int hz_bitstream_write_seek(hz_bitstream_write_t *stream, unsigned int position)
{
	return fseek(stream->file, position, SEEK_SET) != 0;
}











hz_bitstream_writeblocks_t *hz_bitstream_write_allocblocks(void)
{
	hz_bitstream_writeblocks_t *blocks;
	blocks = malloc(sizeof(hz_bitstream_writeblocks_t));
	memset(blocks, 0, sizeof(hz_bitstream_writeblocks_t));
	blocks->blocks = malloc(sizeof(hz_bitstream_writeblock_t));
	blocks->blocks->size = 0;
	blocks->blocks->next = NULL;
	return blocks;
}

void hz_bitstream_write_freeblocks(hz_bitstream_writeblocks_t *blocks)
{
	hz_bitstream_writeblock_t *b, *n;
	b = blocks->blocks;
	while (b)
	{
		n = b->next;
		free(b);
		b = n;
	}
	free(blocks);
}

void hz_bitstream_write_clearblocks(hz_bitstream_writeblocks_t *blocks)
{
	hz_bitstream_writeblock_t *b;
	b = blocks->blocks;
	while (b)
	{
		b->size = 0;
		b = b->next;
	}
}

void hz_bitstream_write_writeblocks(hz_bitstream_writeblocks_t *blocks, hz_bitstream_write_t *stream)
{
	hz_bitstream_writeblock_t *b;
	hz_bitstream_write_flushbits(blocks);
	b = blocks->blocks;
	while (b)
	{
		// FIXME: do something if this errors (fwrite() != b->size)
		if (b->size)
		{
			fwrite(b->data, 1, b->size, stream->file);
			b->size = 0;
		}
		b = b->next;
	}
}

unsigned int hz_bitstream_write_sizeofblocks(hz_bitstream_writeblocks_t *blocks)
{
	hz_bitstream_writeblock_t *b;
	unsigned int size;
	size = 0;
	b = blocks->blocks;
	while (b)
	{
		size += b->size;
		b = b->next;
	}
	return size;
}

void hz_bitstream_write_byte(hz_bitstream_writeblocks_t *blocks, unsigned int num)
{
	hz_bitstream_writeblock_t *b;
	b = blocks->blocks;
	for(;;)
	{
		if (b->size < HZWRITEBLOCKSIZE)
		{
			b->data[b->size++] = num;
			return;
		}
		if (b->next == NULL)
		{
			b->next = malloc(sizeof(hz_bitstream_writeblock_t));
			b = b->next;
			b->next = NULL;
			b->size = 1;
			b->data[0] = num;
			return;
		}
		else
			b = b->next;
	}
}

void hz_bitstream_write_short(hz_bitstream_writeblocks_t *blocks, unsigned int num)
{
	hz_bitstream_write_byte(blocks, (num >>  8) & 0xFF);
	hz_bitstream_write_byte(blocks, (num      ) & 0xFF);
}

void hz_bitstream_write_int(hz_bitstream_writeblocks_t *blocks, unsigned int num)
{
	hz_bitstream_write_byte(blocks, (num >> 24) & 0xFF);
	hz_bitstream_write_byte(blocks, (num >> 16) & 0xFF);
	hz_bitstream_write_byte(blocks, (num >>  8) & 0xFF);
	hz_bitstream_write_byte(blocks, (num      ) & 0xFF);
}

void hz_bitstream_write_bytes(hz_bitstream_writeblocks_t *blocks, void *data, unsigned int size)
{
	int i;
	unsigned char *b;
	b = data;
	for (i = 0;i < size;i++)
		hz_bitstream_write_byte(blocks, b[i]);
}

void hz_bitstream_write_bit(hz_bitstream_writeblocks_t *blocks, unsigned int bit)
{
	blocks->store = (blocks->store << 1) | (bit & 1);
	blocks->count++;
	while (blocks->count >= 8)
	{
		blocks->count -= 8;
		hz_bitstream_write_byte(blocks, (blocks->store >> blocks->count) & 0xFF);
	}
}

void hz_bitstream_write_bits(hz_bitstream_writeblocks_t *blocks, unsigned int num, unsigned int size)
{
	if (size > 24)
	{
		// we can't handle more than 24 bits at a time, so call self with high bits and then handle the rest
		hz_bitstream_write_bits(blocks, num >> 24, size - 24);
		size = 24;
	}
	if (size < 32)
		num &= (1u << size) - 1;
	blocks->store = (blocks->store << size) | num;
	blocks->count += size;
	while (blocks->count >= 8)
	{
		blocks->count -= 8;
		hz_bitstream_write_byte(blocks, (blocks->store >> blocks->count) & 0xFF);
	}
}

void hz_bitstream_write_flushbits(hz_bitstream_writeblocks_t *blocks)
{
	// slam into upper end of 32bit buffer
	blocks->store <<= (32 - blocks->count);
	while (blocks->count > 0)
	{
		hz_bitstream_write_byte(blocks, blocks->store >> 24);
		blocks->store <<= 8;
		blocks->count -= 8;
	}
	blocks->store = 0;
	blocks->count = 0;
}













hzhuffmanwritetree_t *hz_huffman_write_newtree(unsigned int maxsymbols)
{
	hzhuffmanwritetree_t *h;
	int maxnodes, size;
	maxnodes = maxsymbols * 2 - 1;
	size = sizeof(hzhuffmanwritetree_t) + sizeof(hzhuffmanwritenode_t) * maxnodes;
	h = malloc(size);
	memset(h, 0, size);
	h->maxsymbols = maxsymbols;
	h->maxnodes = maxnodes;
	h->mark = 1;
	h->nodes = (hzhuffmanwritenode_t *)(h + 1);
	h->root = NULL;
	return h;
}

void hz_huffman_write_freetree(hzhuffmanwritetree_t *h)
{
	if (h)
	{
		// nodes was allocated as part of the struct so we don't need to free it
		free(h);
	}
}

void hz_huffman_write_clearcounts(hzhuffmanwritetree_t *h)
{
	h->root = NULL;
	h->mark = 1;
	memset(h->nodes, 0, sizeof(hzhuffmanwritenode_t) * h->maxnodes);
}

void hz_huffman_write_countsymbol(hzhuffmanwritetree_t *h, unsigned int num)
{
	if (num >= h->maxsymbols)
		// error!
		return;
	h->nodes[num].count++;
}

void hz_huffman_write_uncountsymbol(hzhuffmanwritetree_t *h, unsigned int num)
{
	if (num >= h->maxsymbols)
		// error!
		return;
	h->nodes[num].count--;
}

static int hz_huffman_write_buildtreefromlengths(hzhuffmanwritetree_t *h)
{
	int i, bits, mask;
	unsigned int start[HZWRITEMAXSYMBOLBITS + 1], lengths[HZWRITEMAXSYMBOLBITS + 1];
	hzhuffmanwritenode_t *node, *freenode, *endofsymbols, *p;

	if (h->maxsymbols > 1)
	{
		// to allow iterating through the node array without a counter
		endofsymbols = freenode = h->nodes + h->maxsymbols;

		// this code computes the bit strings given simply a set of symbol lengths
		memset(lengths, 0, sizeof(lengths));
		for (node = h->nodes;node < endofsymbols;node++)
			lengths[node->length]++;

		// generate the bit strings
		start[0] = start[1] = 0;
		for (i = 1;i < HZWRITEMAXSYMBOLBITS;i++)
			start[i + 1] = (start[i] + lengths[i]) << 1;

		bits = 0;
		for (i = HZWRITEMAXSYMBOLBITS;i >= 1;i--)
		{
			if ((lengths[i] + bits) & 1)
				return HZWRITEERROR_INVALIDTREE;
			bits = (lengths[i] + bits) >> 1;
		}

		for (node = h->nodes;node < endofsymbols;node++)
			node->bits = start[node->length]++;

		if (start[HZWRITEMAXSYMBOLBITS] & (((unsigned int) 1 << HZWRITEMAXSYMBOLBITS) - 1))
			return HZWRITEERROR_INVALIDTREE;

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

	return HZWRITEERROR_OK;
}

void hz_huffman_write_buildtree(hzhuffmanwritetree_t *h)
{
	int nodecount, toolong, bestcount, highestlength;
	hzhuffmanwritenode_t *node, *freenode, *endnode, *endofsymbols, *list, *p, *l;

	// to allow iterating through the node array without a counter
	endnode = h->nodes + h->maxnodes;
	endofsymbols = freenode = h->nodes + h->maxsymbols;

	if (h->maxsymbols > 1)
	{
		// loop until tree contains no codes longer than HZWRITEMAXSYMBOLBITS
		for (;;)
		{
			// clear everything but counts on all nodes
			freenode = endofsymbols;
			for (node = h->nodes;node < endnode;node++)
			{
				node->next = NULL;
				node->parent = NULL;
				node->children[0] = NULL;
				node->children[1] = NULL;
				node->bits = 0;
				node->length = 0;
				node->mark = 0;
				// clear counts on non-leaf nodes
				if (node >= endofsymbols)
					node->count = 0;
			}

			nodecount = 0;
			// make the initial list
			list = NULL;
			for (node = h->nodes;node < endofsymbols;node++)
			{
				if (node->count)
				{
					nodecount++;
					p = NULL;
					l = list;
					while(l && l->count < node->count)
					{
						p = l;
						l = l->next;
					}
					if (p) // if there's a previous, link to the new node
						p->next = node;
					else // otherwise it's the new head of the list
						list = node;
					node->next = l;
				}
			}

			// no nodes
			if (list == NULL)
			{
				h->root = NULL;
				return;
			}

			// the loop ends when there is only one node left, it will be the root
			while (list->next)
			{
				nodecount += 1 - 2;
				// grab a free node and make it the parent of the two chosen
				// note: there are never more non-leaf nodes than leaf nodes,
				//       so there is not a danger of buffer overrun here,
				//       as long as the maxnodes is maxsymbols * 2.
				freenode->count = list->count + list->next->count;
				freenode->parent = NULL;
				// put the higher count one in 0s, so there are more 0s in the
				// resulting file (rather than 1s; this is an arbitrary decision)
				// note: this is undone by the bitstring generation done later
				freenode->children[0] = list->next;
				freenode->children[1] = list;
				freenode->children[0]->parent = freenode;
				freenode->children[1]->parent = freenode;
				// remove the two items from the list
				list = list->next->next;
				// add to the list
				p = NULL;
				l = list;
				while(l && l->count < freenode->count)
				{
					p = l;
					l = l->next;
				}
				if (p) // if there's a previous, link to the new node
					p->next = freenode;
				else // otherwise it's the new head of the list
					list = freenode;
				freenode->next = l;
				// advance to next free node item
				freenode++;
			}

			nodecount = 0;
			for (node = h->nodes;node < endnode;node++)
				if (node->count)
					nodecount++;

			// store the root for future use
			h->root = list;

			// build bit length values
			toolong = 0;
			for (node = h->nodes;node < endofsymbols;node++)
			{
				node->length = 0;
				if (node->count)
				{
					for (p = node;p->parent;p = p->parent)
						node->length++;
				}
				if (node->length > HZWRITEMAXSYMBOLBITS)
					toolong = 1;
			}

			if (!toolong)
				break;

			printf("toolong\n");
			// if we reach this point it means some symbols are longer than
			// HZWRITEMAXSYMBOLBITS we have to mangle the counts and rebuild the tree
			// to solve this situation (the tree storage only supports up to
			// HZWRITEMAXSYMBOLBITS bits per symbol, and this is also done to be nicer
			// to the decoder)

			// find longest valid length used (valid meaning <= HZWRITEMAXSYMBOLBITS)
			// (this usually (always?) answers with HZWRITEMAXSYMBOLBITS)
			highestlength = 0;
			for (node = h->nodes;node < endofsymbols;node++)
				if (highestlength < node->length && node->length <= HZWRITEMAXSYMBOLBITS)
					highestlength = node->length;

			// find most frequent (least likely to have a long bit string) symbol
			// of the length determined above
			bestcount = 0;
			for (node = h->nodes;node < endofsymbols;node++)
				if (node->length == highestlength && bestcount < node->count)
					bestcount = node->count;

			// set all invalid symbols to that count, this forces them to sort
			// 'equally' to that valid symbol
			for (node = h->nodes;node < endofsymbols;node++)
				if (node->length >= highestlength)
					node->count = bestcount;

			// do it all over again with the altered counts to generate a tree
			// (hopefully) containing no codes longer than HZWRITEMAXSYMBOLBITS
		}
	}

	// tree has been successfully limited to HZWRITEMAXSYMBOLBITS, now do the rest
	// of the processing
	hz_huffman_write_buildtreefromlengths(h);

	// validate the generated tree
	for (node = h->nodes;node < endofsymbols;node++)
	{
		if (node->length)
		{
			int bit;
			bit = 1 << (node->length - 1);
			l = h->root;
			while (l >= endofsymbols)
			{
				l = l->children[(node->bits & bit) != 0];
				bit >>= 1;
			}
			if (l != node)
				printf("bitstring does not lead back to symbol!\n");
		}
	}
}

void hz_huffman_write_writetree(hz_bitstream_writeblocks_t *blocks, hzhuffmanwritetree_t *h)
{
	int i, l, bits;

	bits = 0;
	if (h->maxsymbols > 1)
	{
		for (i = 0;i < h->maxsymbols;)
		{
			hz_bitstream_write_bits(blocks, h->nodes[i].length, HZWRITESYMBOLLENGTHBITS);
			bits += HZWRITESYMBOLLENGTHBITS;
#if 0
			i++;
			l = l;
#else
			l = h->nodes[i].length;
			while ((++i) < h->maxsymbols && h->nodes[i].length == l)
			{
				hz_bitstream_write_bit(blocks, 0);
				bits++;
			}
			if (i < h->maxsymbols)
			{
				hz_bitstream_write_bit(blocks, 1);
				bits++;
			}
#endif
		}
	}
}

void hz_huffman_write_writesymbol(hz_bitstream_writeblocks_t *blocks, hzhuffmanwritetree_t *h, unsigned int num)
{
	hzhuffmanwritenode_t *node;
	if (num >= h->maxsymbols)
		// error!
		return;
	node = h->nodes + num;
	// tree is limited to HZWRITEMAXSYMBOLBITS now, so we can always write out the symbol the easy way
#if 1
	if (h->nodes[num].length)
		hz_bitstream_write_bits(blocks, h->nodes[num].bits, h->nodes[num].length);
#else
	if (node->length <= 32)
		hz_bitstream_write_bits(blocks, h->nodes[num].bits, h->nodes[num].length);
	else
	{
		// really huge bitstring!

		// mark a trail of breadcrumbs up the tree
		h->mark++;
		while (node->parent)
		{
			node->mark = h->mark;
			node = node->parent;
		}

		// follow them back down, writing bits
		bits = 0;
		bitcount = 0;
		while (node >= endofsymbols)
		{
			bitcount++;
			if (node->children[1]->mark == node->mark)
			{
				bits |= 1 << (16 - bitcount);
				node = node->children[1];
			}
			else
				node = node->children[0];
			if (bitcount >= 16)
			{
				hz_bitstream_write_bits(blocks, bits, 16);
				bits = 0;
				bitcount = 0;
			}
		}
		if (bitcount)
		{
			bits >>= 16 - bitcount;
			hz_bitstream_writebits(blocks, bits, bitcount);
		}
	}
#endif
}

int hz_huffman_write_getsymbollength(hzhuffmanwritetree_t *h, unsigned int num)
{
	if (num >= h->maxsymbols)
		// error!
		return -1;
	return h->nodes[num].length;
}
