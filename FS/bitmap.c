#include "bitmap.h"
#include <stdio.h>

// converts a block index to an index in the array,
// and a uint8_t that indicates the offset of the bit inside the array.
 BitMapEntryKey BitMap_blockToIndex(int num) {
	BitMapEntryKey entry_key;
	entry_key.entry_num = num / NUMBITS;
	entry_key.bit_num = num % NUMBITS;
	return entry_key;
	
}

// converts a bit to a linear index
 int BitMap_indexToBlock(int entry, uint8_t bit_num) {
	int num = (entry * NUMBITS) + bit_num;
	return num;
}

// given a block_num
// returns if the bit int he bitmap corresponding to the block is set (1) or not (0)
uint8_t BitMap_isBitSet(BitMap* bmap, int block_num) {
	int entry_num = block_num / NUMBITS;
	uint8_t bit_num = block_num % NUMBITS;
	uint8_t e = bmap->entries[entry_num];
	int ret = (e) & (1 << (NUMBITS - 1 - bit_num));
	if (ret != 0) return OCCUPIED;
	else return FREE;
}

// returns the pos of the first bit equal to status in a byte called num
// returns -1 in case of bit not found
int BitMap_check(uint8_t num, int status) {
	if (num < 0) return ERROR_RESEARCH_FAULT;
	int i = 7;
	while (i >= 0) {
		if (status != 0 && (num & (1 << i))) {
			return (NUMBITS - 1 - i);			
		}
		else if (status == 0 && !(num & (1 << i))) {
			return (NUMBITS -1 - i);
		}	
	
		--i;
	}
	return ERROR_RESEARCH_FAULT;
}

// returns the index of the first bit having status "status"
// in the bitmap bmap, and starts looking from position start.
// for humans: returns the global position of that bit we're looking for
// starting by the bitmap cell with index "start".
int BitMap_get(BitMap* bmap, int start, int status) {
	int i = start;
	while (i < bmap->num_bits) {
		int pos = BitMap_check((bmap->entries)[i], status);
		if (pos != ERROR_RESEARCH_FAULT) return (i * NUMBITS + pos);
		i++;
	}
	return ERROR_RESEARCH_FAULT;
}

// sets the bit in bmap at index pos in the blocks list to status
int BitMap_set(BitMap* bmap, int pos, int status) {
	int array_index = pos / NUMBITS;
	int offset = pos % NUMBITS;
	if (status) {
		return (bmap->entries)[array_index] |= (status << (NUMBITS -1 - offset));
	}
	else if (!status) {
		return (bmap->entries)[array_index] &= ~(1 << (NUMBITS -1 - offset));
	}
	else
		return ERROR_RESEARCH_FAULT;
}

// gets all the free bits of the bitmap
int BitMap_getFreeBlocks(BitMap* bmap) {
	int count = 0;
	int tot_bits = bmap->num_bits * NUMBITS;
	for (int i = 0; i < tot_bits; ++i) {
		if (!BitMap_isBitSet(bmap, i)) ++count;
	}
	return count;
}
