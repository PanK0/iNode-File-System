#include "inodefs.h"

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* iNodeFS_init(iNodeFS* fs, DiskDriver* disk) {
	printf ("INIT\n");
	return NULL;
}

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the iNodeFS struct
// and set to the top level directory
void iNodeFS_format(iNodeFS* fs) {
	printf ("FORMAT\n");
}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a iNode block of type FIL
FileHandle* iNodeFS_createFile(DirectoryHandle* d, const char* filename);

// reads in the (preallocated) blocks array, the name of all files in a directory 
int iNodeFS_readDir(char** names, DirectoryHandle* dirhandle);

// opens a file in the  directory d. The file should be exisiting
FileHandle* iNodeFS_openFile(DirectoryHandle* d, const char* filename);

// closes a file handle (destroyes it)
// RETURNS 0 on success, -1 if fails
int iNodeFS_close(FileHandle* f);

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
