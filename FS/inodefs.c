#include "inodefs.h"

// initializes a file system on an already made disk
// returns a handle to the top level directory stored in the first block
DirectoryHandle* iNodeFS_init(iNodeFS* fs, DiskDriver* disk) {
	fs->disk = disk;
	
	// creating the directory handle and filling it
	DirectoryHandle* handle = (DirectoryHandle*) malloc(sizeof(DirectoryHandle));
	iNode* firstdir = (iNode*) malloc(sizeof(iNode));
	
	// If operating on a new disk return NULL: we need to format it
	int snorlax = DiskDriver_readBlock(disk, firstdir, 0);
	if (snorlax) return NULL;
	
	// Filling the handle
	handle->infs = fs;
	handle->dcb = firstdir;
	handle->directory = NULL;
	handle->indirect = NULL;
	handle->current_block = &firstdir->header;
	handle->pos_in_node = 0;
	handle->pos_in_block = 0;
	
	return handle;
}

// creates the inital structures, the top level directory
// has name "/" and its control block is in the first position
// it also clears the bitmap of occupied blocks on the disk
// the current_directory_block is cached in the iNodeFS struct
// and set to the top level directory
void iNodeFS_format(iNodeFS* fs) {
	int num_blocks = fs->disk->header->num_blocks;
	for (int i = 0; i < num_blocks; ++i) {
		DiskDriver_freeBlock(fs->disk, i);
	}
	
	// Once the disk is free, creating the directory header
	BlockHeader header;
	header.block_in_file = TBA;
	header.block_in_node = TBA;
	header.block_in_disk = 0;
	
	// Creating the ICB
	iNodeControlBlock icb;
	icb.directory_block = TBA;
	icb.block_in_disk = header.block_in_disk;
	icb.upper = TBA;
	icb.node_type = DIR;
	
	// Creating the FCB
	FileControlBlock fcb;
	fcb.size_in_bytes = BLOCK_SIZE;
	fcb.size_in_blocks = 1;
	fcb.icb = icb;
	strcpy(fcb.name, "/");
	
	// Creating the First Directory Block
	iNode firstdir;
	firstdir.header = header;
	firstdir.fcb = fcb;
	firstdir.num_entries = 0;
	firstdir.single_indirect = TBA;
	firstdir.double_indirect = TBA;
	int size = (BLOCK_SIZE - sizeof(BlockHeader) - sizeof(FileControlBlock) - sizeof(int) - sizeof(int) - sizeof(int)) / sizeof(int);
	for (int i = 0; i < size; ++i) {
		firstdir.file_blocks[i] = TBA;
	}
	
	// Writing all the content on the disk
	DiskDriver_writeBlock(fs->disk, &firstdir, 0);
	
}

// Duplicates a directory handle
DirectoryHandle* AUX_duplicate_dirhandle(DirectoryHandle* d) {
	DirectoryHandle* daux = (DirectoryHandle*) malloc(sizeof(DirectoryHandle));
	daux->infs = d->infs;
	daux->dcb = d->dcb;
	daux->directory = d->directory;
	daux->indirect = d->indirect;
	daux->current_block = d->current_block;
	daux->pos_in_node = d->pos_in_node;
	daux->pos_in_block = d->pos_in_block;
	
	return daux;
}

// creates an empty file in the directory d
// returns null on error (file existing, no free blocks)
// an empty file consists only of a iNode block of type FIL

// RESEARCH FOR ALREADY EXISTENT FILE
// while searching save the first free occurrence in copy of dirhandle
// 1. search in the node's index list
// 2. search in single indirect (if exists)
// 3 search in double indirect (if exists)

// FILE CREATION
// 1. check the position of the first free occurrence
// 2. do something
FileHandle* iNodeFS_createFile(DirectoryHandle* d, const char* filename) {
	
	// Preliminary stuffs
	if (d == NULL) return NULL;
	if (d->infs == NULL) return NULL;
	DiskDriver* disk = d->infs->disk;
	if (d == NULL) return NULL;
	
	// Creating an iNode struct to store what I need
	iNode* aux_node = (iNode*) malloc(sizeof(iNode));
	int snorlax = TBA;
	
	// Creating a copy of the dirhandle and searching for an already existent file
	// daux->pos_in_node is set to TBA to lately check if there's need to extend the node
	int search_flag = TBA;
	DirectoryHandle* daux = AUX_duplicate_dirhandle(d);
	daux->pos_in_node = TBA;
	
	// Search in the inode
	for (int i = 0; i < inode_idx_size; ++i) {
		if (d->dcb->file_blocks[i] == TBA) {
			// Update the flag and save the first free occurrence
			if (search_flag == TBA) {
				search_flag = 0;
				daux->pos_in_node = i;
				daux->pos_in_block = 0;
			}
		}
		// Reading the node to check for equal file name
		// if snorlax == TBA, the block is free according to the bitmap
		// else check the node name
		else {
			snorlax = DiskDriver_readBlock(disk, aux_node, d->dcb->file_blocks[i]);
			if (snorlax != TBA) {
				if (aux_node->fcb.icb.node_type == FIL && strcmp(aux_node->fcb.name, filename) == 0) {
					printf ("FILE %s ALREADY EXISTS. CREATION FAILED @ iNodeFS_createFile()\n", filename);
								
					// Freeing memory
					aux_node = NULL;
					free (aux_node);		
					daux = NULL;
					free(daux);
		
					return NULL;
				}
			}
		}		
	}
	
	// At this point is sure that does not exists a file with the same filename.
	// Check if there's need to extend the node
	if (daux->pos_in_node == TBA) {
		printf ("NEED TO EXTEND THE NODE! @ iNodeFS_createFile()\n");
		
		// Freeing memory
		aux_node = NULL;
		free (aux_node);		
		daux = NULL;
		free(daux);
		
		return NULL;
	}
	
	// Creation time
	memset(aux_node, 0, BLOCK_SIZE);
	int voyager = DiskDriver_getFreeBlock(disk, 0);
	if (voyager == TBA) {
		printf ("ERROR - DISK COULD BE FULL @ iNodeFS_createFile()\n");
		
		// Freeing memory		
		daux = NULL;
		free(daux);
		
		return NULL;
	}
	
	// Header creation
	BlockHeader header;
	header.block_in_file = TBA;
	header.block_in_node = daux->pos_in_node;
	header.block_in_disk = voyager;
	
	// iNode Control Block creation
	iNodeControlBlock icb;
	icb.directory_block = d->dcb->header.block_in_disk;
	icb.block_in_disk = voyager;
	icb.upper = TBA;
	icb.node_type = FIL;
	
	// File Control Block creation
	FileControlBlock fcb;
	fcb.size_in_bytes = BLOCK_SIZE;
	fcb.size_in_blocks = 1;
	fcb.icb = icb;
	strcpy(fcb.name, filename);
	
	// Compacting all
	aux_node->header = header;
	aux_node->fcb = fcb;
	aux_node->num_entries = 0;
	aux_node->single_indirect = TBA;
	aux_node->double_indirect = TBA;
	for (int i = 0; i < inode_idx_size; ++i) {
		aux_node->file_blocks[i] = TBA;
	}
	
	// Writing on the disk
	snorlax = DiskDriver_writeBlock(disk, aux_node, aux_node->header.block_in_disk);
	if (snorlax == TBA) {
		printf ("ERROR WRITING AUX NODE ON THE DISK @ iNodeFS_createFile()\n");
		
		// Freeing memory
		aux_node = NULL;
		free (aux_node);		
		daux = NULL;
		free(daux);
		
		return NULL;
	}
	
	// Creating the filehandle
	FileHandle* filehandle = (FileHandle*) malloc(sizeof(FileHandle));
	filehandle->infs = d->infs;
	filehandle->fcb = aux_node;
	filehandle->directory = d->dcb;
	filehandle->indirect = d->indirect;
	filehandle->current_block = &(aux_node->header);
	filehandle->pos_in_node = 0;
	filehandle->pos_in_block = 0;
	
	// Updating DCB and dirhandle
	d->dcb->file_blocks[daux->pos_in_node] = voyager;
	d->dcb->num_entries += 1;
	snorlax = DiskDriver_writeBlock(disk, d->dcb, d->dcb->header.block_in_disk);
	if (snorlax == TBA) {
		printf ("ERROR UPDATING DCB ON THE DISK @ iNodeFS_createFile()\n");
		return NULL;
	}
	d->pos_in_node = daux->pos_in_node;
	
	// Freeing memory	
	daux = NULL;
	free(daux);
	
	return filehandle;
}

// reads in the (preallocated) blocks array, the name of all files in a directory 
int iNodeFS_readDir(char** names, DirectoryHandle* d) {
	
	// Preliminary stuffs
	if (d == NULL) return TBA;
	if (d->infs == NULL) return TBA;
	DiskDriver* disk = d->infs->disk;
	if (d == NULL) return TBA;
	
	// Creating an iNode struct to store what I need
	iNode* aux_node = (iNode*) malloc(sizeof(iNode));
	int snorlax = TBA;
	
	// Search in the inode
	// if snorlax == TBA, the block is free according to the bitmap
	// else check the node name
	// j is the index that refers to names array
	int j = 0;
	for (int i = 0; i < inode_idx_size; ++i) {
		if (d->dcb->file_blocks[i] != TBA) {
			snorlax = DiskDriver_readBlock(disk, aux_node, d->dcb->file_blocks[i]);
			if (snorlax != TBA) {
				strcpy(names[j], aux_node->fcb.name);
				++j;
			}
		}					
	}
	
	// Freeing memory
	aux_node = NULL;
	free (aux_node);
	
	return j;
}

// opens a file in the  directory d. The file should be exisiting
FileHandle* iNodeFS_openFile(DirectoryHandle* d, const char* filename) {
	
	// Preliminary stuffs
	if (d == NULL) return NULL;
	if (d->infs == NULL) return NULL;
	DiskDriver* disk = d->infs->disk;
	if (d == NULL) return NULL;
	
	// Creating an iNode struct to store what I need
	iNode* aux_node = (iNode*) malloc(sizeof(iNode));
	int snorlax = TBA;
	
	// Creating the filhandle
	FileHandle* filehandle = (FileHandle*) malloc(sizeof(FileHandle));
	filehandle->infs = d->infs;
	filehandle->fcb = NULL;
	filehandle->directory = d->dcb;
	filehandle->indirect = NULL;
	filehandle->current_block = NULL;
	filehandle->pos_in_node = 0;
	filehandle->pos_in_block = 0;
	
	// Search in the inode
	// if snorlax == TBA, the block is free according to the bitmap
	// else check the node name
	for (int i = 0; i < inode_idx_size; ++i) {
		if (d->dcb->file_blocks[i] != TBA) {
			snorlax = DiskDriver_readBlock(disk, aux_node, d->dcb->file_blocks[i]);
			if (snorlax != TBA) {
				if (strcmp(aux_node->fcb.name, filename) == 0 && aux_node->fcb.icb.node_type == FIL) {
					filehandle->fcb = aux_node;
					filehandle->current_block = &(aux_node->header);
					
					return filehandle;
				}
			}
		}					
	}
	
	
	// Freeing memory
	filehandle = NULL;
	free (filehandle);
	aux_node = NULL;
	free (aux_node);
	
	printf ("FILE %s DOES NOT EXISTS\n", filename);
	return NULL;
}

// closes a file handle (destroyes it)
// RETURNS 0 on success, -1 if fails
int iNodeFS_close(FileHandle* f) {
	
	// Preliminary stuffs
	if (f == NULL) return TBA;
	if (f->infs == NULL) return TBA;
	if (f->infs->disk == NULL) return TBA;
	
	// Closing
	f = NULL;
	free (f);
	
	return 0;
}

// writes in the file, at current position for size bytes stored in data
// overwriting and allocating new space if necessary
// returns the number of bytes written
// 1. check if we are in the firstfileblock. IF so, then: 
// 1.2. take the FileBlock in position f->pos_in_node. IF there's nothing, then create it.
// 1.3. write the things. Change the FileBlock (and create it) when it's necessary

// 2. if we are in an indirect node (f->indirect != NULL) :
// 2.1 write in the node until it's full
// 2.2 go back to upper level (stored in icb.upper)
// 2.3 if we are in an indirect node means that we are in the double indirect: create another NOD and continue to work
// 2.4 if we are in the parent node (icb.upper == TBA) then create a double indirect
int iNodeFS_write(FileHandle* f, void* data, int size) {
	

}

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
int iNodeFS_changeDir(DirectoryHandle* d, char* dirname) {
	
	// Preliminary stuffs
	if (d == NULL) return TBA;
	if (d->infs == NULL) return TBA;
	DiskDriver* disk = d->infs->disk;
	if (d == NULL) return TBA;
	
	// Creating an iNode struct to store what I need
	iNode* aux_node = (iNode*) malloc(sizeof(iNode));
	int snorlax = TBA;
	
	// Going back to parent directory
	if (strcmp(dirname, "..") == 0) {
		
		if (d->directory == NULL) return 0;
		
		snorlax = DiskDriver_readBlock(disk, aux_node, d->directory->header.block_in_disk);
		if (snorlax != TBA) {		
			d->dcb = d->directory;
			d->directory = NULL;
			d->current_block = &(aux_node->header);
			
			return 0;
		}
	}
	
	// Search in the inode
	// if snorlax == TBA, the block is free according to the bitmap
	// else check the node name
	for (int i = 0; i < inode_idx_size; ++i) {
		if (d->dcb->file_blocks[i] != TBA) {
			snorlax = DiskDriver_readBlock(disk, aux_node, d->dcb->file_blocks[i]);
			if (snorlax != TBA) {
				if (strcmp(aux_node->fcb.name, dirname) == 0 && aux_node->fcb.icb.node_type == DIR) {
					d->directory = d->dcb;
					d->dcb = aux_node;
					d->current_block = &(aux_node->header);
										
					return 0;
				}
			}
		}					
	}
	
	// Freeing memory
	aux_node = NULL;
	free (aux_node);
	
	return TBA;
}

// creates a new directory in the current one (stored in fs->current_directory_block)
// 0 on success
// -1 on error
int iNodeFS_mkDir(DirectoryHandle* d, char* dirname) {
	
	// Preliminary stuffs
	if (d == NULL) return TBA;
	if (d->infs == NULL) return TBA;
	DiskDriver* disk = d->infs->disk;
	if (d == NULL) return TBA;
	
	// Creating an iNode struct to store what I need
	iNode* aux_node = (iNode*) malloc(sizeof(iNode));
	int snorlax = TBA;
	
	// Creating a copy of the dirhandle and searching for an already existent file
	// daux->pos_in_node is set to TBA to lately check if there's need to extend the node
	int search_flag = TBA;
	DirectoryHandle* daux = AUX_duplicate_dirhandle(d);
	daux->pos_in_node = TBA;
	
	// Search in the inode
	for (int i = 0; i < inode_idx_size; ++i) {
		if (d->dcb->file_blocks[i] == TBA) {
			// Update the flag and save the first free occurrence
			if (search_flag == TBA) {
				search_flag = 0;
				daux->pos_in_node = i;
				daux->pos_in_block = 0;
			}
		}
		// Reading the node to check for equal file name
		// if snorlax == TBA, the block is free according to the bitmap
		// else check the node name
		else {
			snorlax = DiskDriver_readBlock(disk, aux_node, d->dcb->file_blocks[i]);
			if (snorlax != TBA) {
				if (aux_node->fcb.icb.node_type == DIR && strcmp(aux_node->fcb.name, dirname) == 0) {
					printf ("DIR %s ALREADY EXISTS. CREATION FAILED @ iNodeFS_createFile()\n", dirname);
								
					// Freeing memory
					aux_node = NULL;
					free (aux_node);		
					daux = NULL;
					free(daux);
		
					return TBA;
				}
			}
		}		
	}
	
	// At this point is sure that does not exists a file with the same filename.
	// Check if there's need to extend the node
	if (daux->pos_in_node == TBA) {
		printf ("NEED TO EXTEND THE NODE! @ iNodeFS_createFile()\n");
		
		// Freeing memory
		aux_node = NULL;
		free (aux_node);		
		daux = NULL;
		free(daux);
		
		return TBA;
	}
	
	// Creation time
	memset(aux_node, 0, BLOCK_SIZE);
	int voyager = DiskDriver_getFreeBlock(disk, 0);
	if (voyager == TBA) {
		printf ("ERROR - DISK COULD BE FULL @ iNodeFS_createFile()\n");
		
		// Freeing memory		
		daux = NULL;
		free(daux);
		
		return TBA;
	}
	
	// Header creation
	BlockHeader header;
	header.block_in_file = TBA;
	header.block_in_node = daux->pos_in_node;
	header.block_in_disk = voyager;
	
	// iNode Control Block creation
	iNodeControlBlock icb;
	icb.directory_block = d->dcb->header.block_in_disk;
	icb.block_in_disk = voyager;
	icb.upper = TBA;
	icb.node_type = DIR;
	
	// File Control Block creation
	FileControlBlock fcb;
	fcb.size_in_bytes = BLOCK_SIZE;
	fcb.size_in_blocks = 1;
	fcb.icb = icb;
	strcpy(fcb.name, dirname);
	
	// Compacting all
	aux_node->header = header;
	aux_node->fcb = fcb;
	aux_node->num_entries = 0;
	aux_node->single_indirect = TBA;
	aux_node->double_indirect = TBA;
	for (int i = 0; i < inode_idx_size; ++i) {
		aux_node->file_blocks[i] = TBA;
	}
	
	// Writing on the disk
	snorlax = DiskDriver_writeBlock(disk, aux_node, aux_node->header.block_in_disk);
	if (snorlax == TBA) {
		printf ("ERROR WRITING AUX NODE ON THE DISK @ iNodeFS_createFile()\n");
		
		// Freeing memory
		aux_node = NULL;
		free (aux_node);		
		daux = NULL;
		free(daux);
		
		return TBA;
	}
	
	// Updating DCB and dirhandle
	d->dcb->file_blocks[daux->pos_in_node] = voyager;
	d->dcb->num_entries += 1;
	snorlax = DiskDriver_writeBlock(disk, d->dcb, d->dcb->header.block_in_disk);
	if (snorlax == TBA) {
		printf ("ERROR UPDATING DCB ON THE DISK @ iNodeFS_createFile()\n");
		return TBA;
	}
	d->pos_in_node = daux->pos_in_node;
	
	// Freeing memory	
	daux = NULL;
	free(daux);
	
	return 0;
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int iNodeFS_remove(DirectoryHandle* d, char* filename);
