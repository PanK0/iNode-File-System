#pragma once
#include <stdint.h>

// Errors
#define ERROR_RESEARCH_FAULT	-1

//Just to keep in mind
#define OCCUPIED	1
#define FREE		0
#define NUMBITS		8

typedef struct {
	int num_bits;		// WARNING: THIS IS THE ARRAY DIMENSION. THE NUMBER OF BITS IS num_bits * NUMBITS
	uint8_t* entries;
}  BitMap;

typedef struct {
	int entry_num;
	uint8_t bit_num;
} BitMapEntryKey;

// converts a block index to an index in the array,
// and a uint8_t that indicates the offset of the bit inside the array
BitMapEntryKey BitMap_blockToIndex(int num);

// converts a bit to a linear index
 int BitMap_indexToBlock(int entry, uint8_t bit_num);

// given a block_num
// returns if the bit int he bitmap corresponding to the block is set (1) or not (0)
 uint8_t BitMap_isBitSet(BitMap* bmap, int block_num);

// returns the pos of the first bit equal to status in a byte called num
// returns -1 in case of bit not found
int BitMap_check(uint8_t num, int status);

// returns the index of the first bit having status "status"
// in the bitmap bmap, and starts looking from position start.
// for humans: returns the global position in the bitmap of that bit we're looking for
// starting by the bitmap cell with index "start".
int BitMap_get(BitMap* bmap, int start, int status);

// sets the bit in bmap at index pos in the blocks list to status
int BitMap_set(BitMap* bmap, int pos, int status);

// gets all the free bits of the bitmap
int BitMap_getFreeBlocks(BitMap* bmap);

/*	NOTES
*	Changed char with uint8_t to avoid mistakes
*/
