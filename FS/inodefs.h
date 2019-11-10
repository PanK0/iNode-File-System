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

#define SINGLE		-1
#define DOUBLE		-2

#define READ		0
#define WRITE		1


/********** INFO STRUCTURS **********/

// Header
// Occupies the first portion of each block in the disk
typedef struct {
	int block_in_file;		// absolute position in the file. If -1 it's an iNode
	int block_in_node;		// position in the parent's iNode's array
	int block_in_disk;		// position in the disk
} BlockHeader;

// iNode Control Block
typedef struct {
	int directory_block;	// first node of the parent directory
	int block_in_disk;		// repeated position of the block in the disk
	int upper;				// index of the upper level node. TBA if it's a FIL or DIR
	int node_type;			// NOD for node, FIL for file, DIR for dir
} iNodeControlBlock;

// File Control Block
typedef struct {
	int size_in_bytes;		// size in bytes. Multiple of BLOCK_SIZE
	int size_in_blocks;		// how many blocks this file occupy on the disk
	iNodeControlBlock icb;	// ICB. It's null if we are in a upper level node
	char name[NAME_SIZE];	// name of the file
} FileControlBlock;

/* These two structures are contained in a node. A node has:
 * a header in which to store node's informations
 * a FCB in which to store some file informations
 * some data.
*/ 


/********** iNODE STRUCT **********/

// Node.
// Can be a FIL, or a DIR
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

// Indirect Node.
// Type NOD 
typedef struct {
	BlockHeader header;
	iNodeControlBlock icb;
	int num_entries;
	int file_blocks[ (BLOCK_SIZE
			-sizeof(BlockHeader)
			-sizeof(iNodeControlBlock)
			-sizeof(int)) / sizeof(int) ];
} iNode_indirect;


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
	iNode_indirect* indirect;		// pointer to the current node in the file. Only used if we are in an indexed node
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
	iNode_indirect* indirect;		// pointer to the current node in the file. Only used if we are in an indexed node
	BlockHeader* current_block;		// current block in the file
	int pos_in_node;				// cursor position in the iNode's index list
	int pos_in_block;				// relative position of the cursor in the FileBlock
} FileHandle;


/********** SOME SIZES **********/

int inode_idx_size = (BLOCK_SIZE
			-sizeof(BlockHeader)
			-sizeof(FileControlBlock)
			-sizeof(int)
			-sizeof(int)
			-sizeof(int)) / sizeof(int);
int indirect_idx_size = (BLOCK_SIZE
			-sizeof(BlockHeader)
			-sizeof(iNodeControlBlock)
			-sizeof(int)) / sizeof(int);
int DB_idx_size = (BLOCK_SIZE
			-sizeof(BlockHeader)) / sizeof(int);
int FB_text_size = BLOCK_SIZE - sizeof(BlockHeader);


/********** FILE SYSTEM'S FUNCTIONS **********/

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* iNodeFS_init(iNodeFS* fs, DiskDriver* disk);

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the iNodeFS struct
// and set to the top level directory
void iNodeFS_format(iNodeFS* fs);

// Duplicates a directory handle
DirectoryHandle* AUX_duplicate_dirhandle(DirectoryHandle* d);

// Duplicates a file handle
FileHandle* AUX_duplicate_filehandle(FileHandle* f);

// puts the directoryhandle at the right place in the inode with side effect
// mode == READ or WRITE
void AUX_indirect_dir_management (DirectoryHandle* d, int mode);

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a iNode block of type FIL
FileHandle* iNodeFS_createFile(DirectoryHandle* d, const char* filename);

// reads in the (preallocated) blocks array, the name of all files in a directory 
int iNodeFS_readDir(char** names, DirectoryHandle* d);

// opens a file in the  directory d. The file should be exisiting
FileHandle* iNodeFS_openFile(DirectoryHandle* d, const char* filename);

// closes a file handle (destroyes it)
// RETURNS 0 on success, -1 if fails
int iNodeFS_close(FileHandle* f);

// puts the filehandle at the right place in the inode with side effect
// mode == READ or WRITE
void AUX_indirect_management (FileHandle* f, int mode);

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
int iNodeFS_write(FileHandle* f, void* data, int size);

// reads in the file, at current position size bytes and stores them in data
// returns the number of bytes read
int iNodeFS_read(FileHandle* f, void* data, int size);

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success
// -1 on error (file too short)
int iNodeFS_seek(FileHandle* f, int pos);

// seeks for a directory in d. If dirname is equal to ".." it goes one level up
// 0 on success, negative value on error
// it does side effect on the provided handle
int iNodeFS_changeDir(DirectoryHandle* d, char* dirname);

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success
// -1 on error
int iNodeFS_mkDir(DirectoryHandle* d, char* dirname);

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int iNodeFS_remove(DirectoryHandle* d, char* filename);
