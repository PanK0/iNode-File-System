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

// Duplicates a file handle
FileHandle* AUX_duplicate_filehandle(FileHandle* f) {
	FileHandle* faux = (FileHandle*) malloc(sizeof(FileHandle));
	faux->infs = f->infs;
	faux->fcb = f->fcb;
	faux->directory = f->directory;
	faux->indirect = f->indirect;
	faux->current_block = f->current_block;
	faux->pos_in_node = f->pos_in_node;
	faux->pos_in_block = f->pos_in_block;
	
	return faux;
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


// puts the filehandle at the right place in the inode with side effect
// mode == READ or WRITE
void AUX_indirect_management (FileHandle* f, int mode) {
	
	if (f == NULL) return;
	DiskDriver* disk = f->infs->disk;
	int snorlax = TBA;
	iNode_indirect* aux_node = (iNode_indirect*) malloc(sizeof(iNode_indirect));
	
	// Case 1 : we're in the main node (type FIL), so f->indirect == NULL;
	// if there's need to create an indirect node, create it, write it and locate the filehandle
	if (f->indirect == NULL) {
		// There's space in FIL
		if (f->pos_in_node < inode_idx_size) return;
		// Need to create indirect node
		if (f->fcb->single_indirect == TBA && mode == WRITE) {
			
			int voyager = DiskDriver_getFreeBlock(disk, 0);
			if (voyager == TBA) {
				printf ("ERROR DISK FULL - 1.0 @ AUX_indirect_management()\n");
			}
					
			// Creating BlockHeader
			BlockHeader header;
			header.block_in_file = TBA;
			header.block_in_node = SINGLE;
			header.block_in_disk = voyager;
			
			// Creating icb
			iNodeControlBlock icb;
			icb.directory_block = f->directory->header.block_in_disk;
			icb.block_in_disk = header.block_in_disk;
			icb.upper = f->fcb->header.block_in_disk;
			icb.node_type = NOD;
			
			// NOD stuffs
			aux_node->header = header;
			aux_node->icb = icb;
			aux_node->num_entries = 0;
			for (int i = 0; i < indirect_idx_size; ++i) {
				aux_node->file_blocks[i] = TBA;
			}
			
			// Writing on the disk
			snorlax = DiskDriver_writeBlock(disk, aux_node, aux_node->header.block_in_disk);
			if (snorlax == TBA) {
				printf ("ERROR WRITING - 1.1 @ AUX_indirect_management()\n");
						
				// Freeing memory
				aux_node = NULL;
				free (aux_node);
				return;
			}
			
			// Updating f->fcb
			f->fcb->single_indirect = aux_node->header.block_in_disk;
			f->fcb->fcb.size_in_blocks += 1;
			f->fcb->fcb.size_in_bytes += BLOCK_SIZE;
			snorlax = DiskDriver_writeBlock(disk, f->fcb, f->fcb->header.block_in_disk);
			if (snorlax == TBA) {
				printf ("ERROR WRITING - 1.2 @ AUX_indirect_management()\n");
						
				// Freeing memory
				aux_node = NULL;
				free (aux_node);
				return;
			}
			
			// Updating f
			f->indirect = aux_node;
			f->current_block = &(aux_node->header);
			f->pos_in_node = 0;
			f->pos_in_block = 0;
		}
		// Don't need to create indirect node: it already exists
		// just update f
		else {
			// Updating f
			snorlax = DiskDriver_readBlock(disk, aux_node, f->fcb->single_indirect);
			if (snorlax == TBA) {
				printf ("ERROR READING - 1.3 @ AUX_indirect_management()\n");
						
				// Freeing memory
				aux_node = NULL;
				free (aux_node);
				return;
			}
			f->indirect = aux_node;
			f->current_block = &(aux_node->header);
			f->pos_in_node = 0;
			f->pos_in_block = 0;
		}
		
	}
		// 1. verify if we are in single indirect or in double indirect
		// 		if in single indirect, f->indirect->icb.upper == f->fcb->header.block_in_disk
		//		AND f->indirect->header.block_in_disk == f->fcb->single_indirect
		// 1.1 if single indirect is full, check for double indirect existance. if exists just update f
		//		if double indirect does not exists, create it and update f
		// 2. verify if we are in double indirect
		//		2 cases possible: 
		//		2.0.1 We are in the double indirect
		//				f->indirect->icb.upper == f->fcb->header.block_in_disk
		//				SO verify if it's full
		//				verify if there's need to create another NOD
		//		2.0.2 We are in a double indirect NOD
		//				f->indirect->icb.upper == f->fcb->double_indirect
		//				SO verify if it's full
		//				take this node's file_blocks[f->pos_in_node] and verify if it already exists
		//				create another if necessary
		// 2.1 
	else {
		// We are in the single indirect
		if (f->indirect->header.block_in_disk == f->fcb->single_indirect && 
			f->indirect->icb.upper == f->fcb->header.block_in_disk) {
			
			if (f->pos_in_node < indirect_idx_size) return;
			else {
				// double indirect also exists: just move there f
				if (f->fcb->double_indirect != TBA) {
					// Updating f
					snorlax = DiskDriver_readBlock(disk, aux_node, f->fcb->double_indirect);
					if (snorlax == TBA) {
						printf ("ERROR READING - 1.4 @ AUX_indirect_management()\n");
								
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					f->indirect = aux_node;
					f->current_block = &(aux_node->header);
					f->pos_in_node = 0;
					f->pos_in_block = 0;
				}
				// double indirect does not exists: create it and move there f
				else if (f->fcb->double_indirect == TBA && mode == WRITE) {
					memset(aux_node, 0, BLOCK_SIZE);
					int voyager = DiskDriver_getFreeBlock(disk, 0);
					if (voyager == TBA) {
						printf ("ERROR DISK FULL - 1.5 @ AUX_indirect_management()\n");
					}
					
					// Creating BlockHeader
					BlockHeader header;
					header.block_in_file = TBA;
					header.block_in_node = DOUBLE;
					header.block_in_disk = voyager;
					
					// Creating icb
					iNodeControlBlock icb;
					icb.directory_block = f->directory->header.block_in_disk;
					icb.block_in_disk = header.block_in_disk;
					icb.upper = f->fcb->header.block_in_disk;
					icb.node_type = NOD;
					
					// NOD stuffs
					aux_node->header = header;
					aux_node->icb = icb;
					aux_node->num_entries = 0;
					for (int i = 0; i < indirect_idx_size; ++i) {
						aux_node->file_blocks[i] = TBA;
					}
					
					// Writing on the disk
					snorlax = DiskDriver_writeBlock(disk, aux_node, aux_node->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - 1.6 @ AUX_indirect_management()\n");
								
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating f->fcb
					f->fcb->double_indirect = aux_node->header.block_in_disk;
					f->fcb->fcb.size_in_blocks += 1;
					f->fcb->fcb.size_in_bytes += BLOCK_SIZE;
					snorlax = DiskDriver_writeBlock(disk, f->fcb, f->fcb->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - 1.7 @ AUX_indirect_management()\n");
								
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating f
					f->indirect = aux_node;
					f->current_block = &(aux_node->header);
					f->pos_in_node = 0;
					f->pos_in_block = 0;
					
				}
			}
			
		}
		// We are in the double indirect
		else if (f->indirect->header.block_in_disk == f->fcb->double_indirect && 
				f->indirect->icb.upper == f->fcb->header.block_in_disk) {
			
			if (f->pos_in_node < indirect_idx_size) {
				// Need to create another NOD, then update f
				if (f->indirect->file_blocks[f->pos_in_block] == TBA && mode == WRITE) {
					memset(aux_node, 0, BLOCK_SIZE);
					int voyager = DiskDriver_getFreeBlock(disk, 0);
					if (voyager == TBA) {
						printf ("ERROR DISK FULL - 1.5 @ AUX_indirect_management()\n");
					}
					
					// Creating BlockHeader
					BlockHeader header;
					header.block_in_file = TBA;
					header.block_in_node = f->pos_in_node;
					header.block_in_disk = voyager;
					
					// Creating icb
					iNodeControlBlock icb;
					icb.directory_block = f->directory->header.block_in_disk;
					icb.block_in_disk = header.block_in_disk;
					icb.upper = f->indirect->header.block_in_disk;
					icb.node_type = NOD;
					
					// NOD stuffs
					aux_node->header = header;
					aux_node->icb = icb;
					aux_node->num_entries = 0;
					for (int i = 0; i < indirect_idx_size; ++i) {
						aux_node->file_blocks[i] = TBA;
					}
					
					// Writing on the disk
					snorlax = DiskDriver_writeBlock(disk, aux_node, aux_node->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - 1.6 @ AUX_indirect_management()\n");
								
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating f->fcb
					f->fcb->double_indirect = aux_node->header.block_in_disk;
					f->fcb->fcb.size_in_blocks += 1;
					f->fcb->fcb.size_in_bytes += BLOCK_SIZE;
					snorlax = DiskDriver_writeBlock(disk, f->fcb, f->fcb->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - 1.7 @ AUX_indirect_management()\n");
								
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating f
					f->indirect = aux_node;
					f->current_block = &(aux_node->header);
					f->pos_in_node = 0;
					f->pos_in_block = 0;
				}
				// double indirect's NOD also exists. Just update f
				else {
					snorlax = DiskDriver_readBlock(disk, aux_node, f->indirect->file_blocks[f->pos_in_block]);
					if (snorlax == TBA) {
						printf ("ERROR READING - 1.8 @ AUX_indirect_management()\n");
								
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating f
					f->indirect = aux_node;
					f->current_block = &(aux_node->header);
					f->pos_in_node = 0;
					f->pos_in_block = 0;
					
				}
			}
			// NO MORE SPACE
			else {
				printf ("TOO LARGE FILE - 1.8 @ AUX_indirect_management()\n");
				return;
			}
			
		}
		/**** TO DO ***/
		
		// We are in a double indirect NOD
		else if (f->indirect->icb.upper == f->fcb->double_indirect && 
				f->indirect->header.block_in_disk != f->fcb->double_indirect) {
			// Check if it's full. if so, move to next one
			
			// NB if I'm in a double_indirect's NOD, all pointers of filehandle refers to it.
			// I can go to the next NOD of double_indirect by looking ad NOD->header.block_in_node
			
			printf ("DISASTER \n");
		}
	}
	
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
	
	// Preliminary stuffs
	if (f == NULL) return TBA;
	if (f->infs == NULL) return TBA;
	DiskDriver* disk = f->infs->disk;
	if (disk == NULL) return TBA;
	if (data == NULL) return TBA;
	
	// Blocks stuffs
	FileBlock* aux_fb = (FileBlock*) malloc(sizeof(FileBlock));
	FileHandle* faux = AUX_duplicate_filehandle(f);
	int snorlax = TBA;
	int voyager = TBA;

	int written_data = 0;
	while (written_data < size ) {
		// Check if we are in the firstfileblock
		if (faux->indirect == NULL) {
			// Write in the block
			if (faux->fcb->file_blocks[faux->pos_in_node] != TBA) {
				snorlax = DiskDriver_readBlock(disk, aux_fb, faux->fcb->file_blocks[faux->pos_in_node]);
				if (snorlax == TBA) {
					printf ("ERROR READING - 1.1 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
				}
				
				// Check if the block is full : if not, write, else move f->pos_in_node.
				// if f->fcb->file_blocks if full we need to create an indexed node
				if (faux->pos_in_block < FB_text_size) {
					faux->current_block = &(aux_fb->header);
					aux_fb->data[faux->pos_in_block] = ((char*)data)[written_data];
					//strncpy(aux_fb->data, (char*)data, size);
					
					//printf ("text: %s, wdata: %d, posinblock: %d, data : %s\n", aux_fb->data, written_data, faux->pos_in_block, (char*)data);
					
					++faux->pos_in_block;
					++written_data;
					
					// write
					snorlax = DiskDriver_writeBlock(disk, aux_fb, aux_fb->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - 1.1.2 @ iNodeFS_write()\n");
						
						// Freeing memory
						aux_fb = NULL;
						free (aux_fb);
						faux = NULL;
						free (faux);
						return TBA;
					}									
				}
				// update the current block and the pointers
				else {
					snorlax = DiskDriver_writeBlock(disk, aux_fb, aux_fb->header.block_in_disk);
					if (snorlax == TBA) {
					printf ("ERROR WRITING - 1.2 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
					}
					
					++faux->pos_in_node;
					faux->pos_in_block = 0;
					
//NB		     	// Verify if the inode's block list is full.
					// If so, it will be created an indirect node (if not present) and the filehandle is moved to indirect node
					AUX_indirect_management(faux, WRITE);
				}				
			}
			// or create it 
			else {
				memset(aux_fb, 0, BLOCK_SIZE);
				voyager = DiskDriver_getFreeBlock(disk, 0);
				if (voyager == TBA) {
					printf ("ERROR DISK FULL - 1.2.1 @ iNodeFS_write()\n");
					return TBA;
				}
				
				// Header creation
				BlockHeader header;
				header.block_in_file = faux->current_block->block_in_file+1;
				header.block_in_node = faux->pos_in_node;
				header.block_in_disk = voyager;
				
				// Setting array
				for (int i = 0; i < FB_text_size; ++i) {
					aux_fb->data[i] = 0;
				}
				
				aux_fb->header = header;
				
				// Writing on disk
				snorlax = DiskDriver_writeBlock(disk, aux_fb, aux_fb->header.block_in_disk);
				if (snorlax == TBA) {
					printf ("ERROR WRITING - 1.3 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
				}
				
				// Updating f->fcb
				faux->fcb->num_entries += 1;
				faux->fcb->file_blocks[faux->pos_in_node] = aux_fb->header.block_in_disk;
				faux->fcb->fcb.size_in_blocks += 1;
				faux->fcb->fcb.size_in_bytes += BLOCK_SIZE;
				snorlax = DiskDriver_writeBlock(disk, faux->fcb, faux->fcb->header.block_in_disk);
				if (snorlax == TBA) {
					printf ("ERROR WRITING - 1.4 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
				}
			
			}
			
		}
		// Check if we are in a single_indirect
		// single_idirect : same thing of first node
		else if (faux->indirect != NULL && faux->indirect->icb.upper == faux->fcb->header.block_in_disk){
			
			// Write int he block...
			if (faux->indirect->file_blocks[faux->pos_in_node] != TBA) {
				snorlax = DiskDriver_readBlock(disk, aux_fb, faux->indirect->file_blocks[faux->pos_in_node]);
				if (snorlax == TBA) {
					printf ("ERROR READING - 1.5 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
				}
				// Check if the block is full : if not, write, else move f->pos_in_node
				// if faux->indirect->file_blocks is full we need to create a double indirect
				if (faux->pos_in_block < FB_text_size) {
					faux->current_block = &(aux_fb->header);
					aux_fb->data[faux->pos_in_block] = ((char*)data)[written_data];
					++faux->pos_in_block;
					++written_data;
					
					// write
					snorlax = DiskDriver_writeBlock(disk, aux_fb, aux_fb->header.block_in_disk);
					if (snorlax == TBA) {
					printf ("ERROR WRITING - 1.1.2 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
					}
					
				}
				// Update the current block and the pointers
				else {
					snorlax = DiskDriver_writeBlock(disk, aux_fb, aux_fb->header.block_in_disk);
					if (snorlax == TBA) {
					printf ("ERROR WRITING - 1.6 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
					}
					
					++faux->pos_in_node;
					faux->pos_in_block = 0;
					
/*NB*/				AUX_indirect_management(faux, WRITE);
				}
			}
			// ...or create it
			else {
				memset(aux_fb, 0, BLOCK_SIZE);
				voyager = DiskDriver_getFreeBlock(disk, 0);
				if (voyager == TBA) {
					printf ("ERROR DISK FULL - 1.7 @ iNodeFS_write()\n");
					return TBA;
				}
				
				// Header Creation
				BlockHeader header;
				header.block_in_file = faux->current_block->block_in_file+1;
				header.block_in_node = faux->pos_in_node;
				header.block_in_disk = voyager;
				
				// Setting array
				for (int i = 0; i < FB_text_size; ++i) {
					aux_fb->data[i] = 0;
				}
				
				aux_fb->header = header;
				
				// Writing on disk
				snorlax = DiskDriver_writeBlock(disk, aux_fb, aux_fb->header.block_in_disk);
				if (snorlax == TBA) {
					printf ("ERROR WRITING - 1.8 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
				}
				
				// Updating faux->fcb
				faux->fcb->num_entries += 1;
				faux->indirect->file_blocks[faux->pos_in_node] = aux_fb->header.block_in_disk;
				faux->fcb->fcb.size_in_blocks += 1;
				faux->fcb->fcb.size_in_bytes += BLOCK_SIZE;
				
				// fcb
				snorlax = DiskDriver_writeBlock(disk, faux->fcb, faux->fcb->header.block_in_disk);
				if (snorlax == TBA) {
					printf ("ERROR WRITING - 1.9 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
				}
				
				// indirect
				snorlax = DiskDriver_writeBlock(disk, faux->indirect, faux->indirect->header.block_in_disk);
				if (snorlax == TBA) {
					printf ("ERROR WRITING - 1.10 @ iNodeFS_write()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
				}
				
			}	
		}
		/*** TO DO ***/
		
		// Check if we are in a double_indirect
		// double_indirect : some problems on the way
//		else if () {
			
//		}
	}
	
	// Updating f with faux
	f->indirect = faux->indirect;
	f->current_block = faux->current_block;
	f->pos_in_node = faux->pos_in_node;
	f->pos_in_block = faux->pos_in_block;
	
	// Freeing memory
	aux_fb = NULL;
	free (aux_fb);
	faux = NULL;
	free (faux);
	
	return written_data;
}

// reads in the file, at current position size bytes and stores them in data
// returns the number of bytes read
int iNodeFS_read(FileHandle* f, void* data, int size) {
	
	// Preliminary stuffs
	if (f == NULL) return TBA;
	if (f->infs == NULL) return TBA;
	DiskDriver* disk = f->infs->disk;
	if (disk == NULL) return TBA;
	if (data == NULL) return TBA;
	
	// Blocks stuffs
	FileBlock* aux_fb = (FileBlock*) malloc(sizeof(FileBlock));
	FileHandle* faux = AUX_duplicate_filehandle(f);
	int snorlax = TBA;
	
	int read_data = 0;
	while (read_data < size) {
		// Check if we are in the firstfileblock		
		if (faux->indirect == NULL) {
			// Read the block
			if (faux->fcb->file_blocks[faux->pos_in_node] != TBA) {
				snorlax = DiskDriver_readBlock(disk, aux_fb, faux->fcb->file_blocks[faux->pos_in_node]);
				if (snorlax == TBA) {
					printf ("ERROR READING - 2.1 @ iNodeFS_read()\n");
					
					// Freeing memory
					aux_fb = NULL;
					free (aux_fb);
					faux = NULL;
					free (faux);
					return TBA;
				}
				
				// Check if the block is full : if not, read, else move f->pos_in_node.
				if (faux->pos_in_block < FB_text_size) {
					faux->current_block = &(aux_fb->header);
					if (aux_fb->data[faux->pos_in_block] == '\0') break;
					((char*)data)[read_data] = aux_fb->data[faux->pos_in_block];
				
					++faux->pos_in_block;
					++read_data;
				}
				// update the pointers
				else {
					++faux->pos_in_node;
					faux->pos_in_block = 0;
					
					AUX_indirect_management(faux, READ);
				}
			}
		}
		// Check if we are in a single_indirect
		//single_idirect : same thing of first node
		else if (faux->indirect != NULL && faux->indirect->icb.upper == faux->fcb->header.block_in_disk){
			snorlax = DiskDriver_readBlock(disk, aux_fb, faux->indirect->file_blocks[faux->pos_in_node]);
			if (snorlax == TBA) {
				printf ("ERROR READING - 2.2 @ iNodeFS_read()\n");
				
				// Freeing memory
				aux_fb = NULL;
				free (aux_fb);
				faux = NULL;
				free (faux);
				return TBA;
			}
			// Check if the block is full : if not, read, else move f->pos_in_node
			if (faux->pos_in_block < FB_text_size) {
				faux->current_block = &(aux_fb->header);
				((char*)data)[read_data] = aux_fb->data[faux->pos_in_block];
				++faux->pos_in_block;
				++read_data;
			}
			// Update the pointers
			else {
				++faux->pos_in_node;
				faux->pos_in_block = 0;
				
				AUX_indirect_management(faux, READ);
			}
		}
		/*** TO DO ***/
		// Check if we are in a double_indirect
		// double_indirect : some problems on the way
	}
	
	// Updating f with faux
	f->indirect = faux->indirect;
	f->current_block = faux->current_block;
	f->pos_in_node = faux->pos_in_node;
	f->pos_in_block = faux->pos_in_block;
	
	// Freeing memory
	aux_fb = NULL;
	free (aux_fb);
	faux = NULL;
	free (faux);
	
	return read_data;
}

// returns the number of bytes read (moving the current pointer to pos)
// returns pos on success
// -1 on error (file too short)
int iNodeFS_seek(FileHandle* f, int pos) {
	
	// Preliminary stuffs
	if (f == NULL) return TBA;
	if (f->infs == NULL) return TBA;
	DiskDriver* disk = f->infs->disk;
	if (disk == NULL) return TBA;
	if (pos < 0) {
		printf ("ERROR NEGATIVE SEEK INPUT - 3.1 @ iNodeFS_seek()\n");
		return TBA;
	}
	
	int snorlax = TBA;
	int pos_in_node = TBA;
	int pos_in_block = TBA;
	int inode_size = inode_idx_size * FB_text_size;
	int indirect_size = indirect_idx_size * FB_text_size;
	int double_indirect_size = indirect_idx_size * indirect_idx_size * FB_text_size;
	
/*	
	f->pos_in_node = 0;
	f->pos_in_block = 0;
	f->indirect = NULL;
	f->current_block = &(f->fcb->header);
*/
		
	// Calculating the position
	if (pos < inode_size) {
		pos_in_node = pos / FB_text_size;
		pos_in_block = pos % FB_text_size;
		
		if (f->fcb->file_blocks[pos_in_node] != TBA) {
			FileBlock* aux_fb = (FileBlock*) malloc(sizeof(FileBlock));
			snorlax = DiskDriver_readBlock(disk, aux_fb, f->fcb->file_blocks[pos_in_node]);
			if (snorlax == TBA) {
				printf ("ERROR READING - 3.2 @ iNodeFS_seek()\n");
				
				// Free memory
				aux_fb = NULL;
				free (aux_fb);
				
				return TBA;
			}
			
			// Updating f
			f->current_block = &(aux_fb->header);
			f->pos_in_node = pos_in_node;
			f->pos_in_block = pos_in_block;
			f->indirect = NULL;
			
			return (pos_in_node + pos_in_block);
		}
		else {
			printf ("ERROR BAD SEEK INPUT - 3.3 @ iNodeFS_seek()\n");
			return TBA;
		}
	}
	// we are in the single_indirect
	else if (pos >= inode_size && pos < (inode_size + indirect_size) ) {
		pos_in_node = (pos - inode_size) / FB_text_size;
		pos_in_block = (pos - inode_size) % FB_text_size;
		
		if (f->fcb->single_indirect != TBA) {
			iNode_indirect* aux_indirect = (iNode_indirect*) malloc(sizeof(iNode_indirect));
			snorlax = DiskDriver_readBlock(disk, aux_indirect, f->fcb->single_indirect);
			if (snorlax == TBA) {
				printf ("ERROR READING - 3.3.2 @ iNodeFS_seek()\n");
				
				// Free memory
				aux_indirect = NULL;
				free (aux_indirect);
				
				return TBA;
			}
			
			if (aux_indirect->file_blocks[pos_in_node] != TBA) {
				FileBlock* aux_fb = (FileBlock*) malloc(sizeof(FileBlock));
				snorlax = DiskDriver_readBlock(disk, aux_fb, f->fcb->file_blocks[pos_in_node]);
				if (snorlax == TBA) {
					printf ("ERROR READING - 3.4 @ iNodeFS_seek()\n");
					
					// Free memory
					aux_indirect = NULL;
					free (aux_fb);
					aux_fb = NULL;
					free (aux_fb);
					
					return TBA;
				}
				
				// Updating f
				f->current_block = &(aux_fb->header);
				f->indirect = aux_indirect;
				f->pos_in_node = pos_in_node;
				f->pos_in_block = pos_in_block;
				
				return (pos_in_node + pos_in_block);
			}
		}
	}
	// we are in a double_indirect
	else if (pos >= (inode_size + indirect_size) && pos < double_indirect_size -1) {
		int pos_in_indirect = (pos - inode_size - indirect_size) / indirect_size;  //Nod in indirect
		pos_in_node = ((pos - inode_size - indirect_size) % FB_text_size) / FB_text_size; // Pos in indirect's indirect
		pos_in_block = ((pos - inode_size - indirect_size) % FB_text_size) % FB_text_size;
		
		if (f->fcb->double_indirect != TBA) {
			iNode_indirect* aux_indirect = (iNode_indirect*) malloc(sizeof(iNode_indirect));
			snorlax = DiskDriver_readBlock(disk, aux_indirect, f->fcb->single_indirect);
			if (snorlax == TBA) {
				printf ("ERROR READING - 3.5 @ iNodeFS_seek()\n");
				
				// Free memory
				aux_indirect = NULL;
				free (aux_indirect);
				
				return TBA;
			}
			
			if (aux_indirect->file_blocks[pos_in_indirect] != TBA) {
				snorlax = DiskDriver_readBlock(disk, aux_indirect, aux_indirect->file_blocks[pos_in_indirect]);
				if (snorlax == TBA) {
					printf ("ERROR READING - 3.5 @ iNodeFS_seek()\n");
					
					// Free memory
					aux_indirect = NULL;
					free (aux_indirect);
					
					return TBA;
				}
				
				if (aux_indirect->file_blocks[pos_in_node] != TBA) {
					FileBlock* aux_fb = (FileBlock*) malloc(sizeof(FileBlock));
					snorlax = DiskDriver_readBlock(disk, aux_fb, aux_indirect->file_blocks[pos_in_node]);
					if (snorlax == TBA) {
						printf ("ERROR READING - 3.6 @ iNodeFS_seek()\n");
						
						// Free memory
						aux_indirect = NULL;
						free (aux_fb);
						aux_fb = NULL;
						free (aux_fb);
						
						return TBA;
					}
					
					
					// Updating f
					f->current_block = &(aux_fb->header);
					f->indirect = aux_indirect;
					f->pos_in_node = pos_in_node;
					f->pos_in_block = pos_in_block;
					
					return (pos_in_indirect + pos_in_node + pos_in_block);
				}
			}
		}
	}
	
	return TBA;
	
}

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
