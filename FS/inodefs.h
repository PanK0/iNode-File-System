#pragma once
#include "disk_driver.c"
#include <string.h>
#include <stdlib.h>

// Node type
#define NOD	-1
#define FIL	 0
#define DIR	 1

// Stuffs
#define FAULT		-1
#define TBA			-1
#define NAME_SIZE	128


/********** INFO STRUCTURS **********/

// Header
// Occupies the first portion of each block in the disk
typedef struct {
	int block_in_file;		// absolute position in the file. If 0 it's an iNode
	int block_in_node;		// position in the parent's iNode's array
	int block_in_disk;		// position in the disk
} BlockHeader;

// File Control Block
typedef struct {
	int directory_block;	// first node of the parent directory
	int block_in_disk;		// repeated position of the block in the disk
	int upper;				// index of the upper level node
	int size_in_bytes;		// size in bytes. Multiple of BLOCK_SIZE
	int size_in_blocks;		// how many blocks this file occupy on the disk
	int node_type;			// NOD for node, FIL for file, DIR for dir
	char name[NAME_SIZE]	// name of the file
} FileControlBlock;

/* These two structures are contained in a node. A node has:
 * a header in which to store node's informations
 * a FCB in which to store some file informations
 * some data.
*/ 


/********** iNODE STRUCT **********/

// Node.
// Can be a NOD, a FIL, or a DIR
// Note that if it's type is NOD, then the node is a sub-level node. It's upper level node is in fcb.upper
typedef struct {
	BlockHeader header;
	FileControlBlock fcb;
	int num_entries;
	int single_indirect;						// A node that stores blocks
	int double_indirect;						// A node that stores nodes that store blocks
	int file_blocks[ (BLOCK_SIZE
			-sizeof(BlockHeader)
			-sizeof(FileControlBlock)
			-sizeof(int)
			-sizeof(int)
			-sizeof(int)) / sizeof(int) ];	
} iNode;


/********** BLOCK STRUCTS *********/

// Directory Block
// Stores an array of int that are iNode indexes
typedef struct {
	BlockHeader header;
	int file_blocks[ (BLOCK_SIZE
			-sizeof(BlockHeader)) / sizeof(int) ];
} DirectoryBlock;

// File Block
// Stores an array of char, that is the content of the file
typedef struct {
	BlockHeader header;
	char data[ BLOCK_SIZE - sizeof(BlockHeader) ];
} FileBlock;


/********** MANAGEMENT STUFFS **********/

// File System struct
typedef struct {
	DiskDriver* disk;
} iNodeFS;

// Directory Handle
// Stores all key informations about the current directory
typedef struct {
	iNodeFS* infs;					// pointer to memory file system struct
	iNode* dcb;						// pointer to the main iNode of the directory
	iNode* directory;				// pointer to the parent directory (null if top level)
	iNode* current_node;			// pointer to the current node in the dierctory
	BlockHeader* current_block;		// current block in the directory
	int pos_in_node;				// cursor position in the iNode's index list
	int pos_in_block;				// relative position of the cursor in the DirectoryBlock
} DirectoryHandle;

// File Handle
// Used to refer to open files
typedef struct {
	iNodeFS* infs;					// pointer to memory file system struct
	iNode* fcb;						// pointer to the main iNode of the file
	iNode* directory;				// pointer to the directory in where the file is stored
	iNode* current_node;			// pointer to the current node in the file;
	BlockHeader* current_block;		// current block in the file
	int pos_in_node;				// cursor position in the iNode's index list
	int pos_in_block;				// relative position of the cursor in the FileBlock
} FileHandle;
