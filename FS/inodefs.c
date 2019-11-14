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

// puts the directoryhandle at the right place in the inode with side effect
// mode == READ or WRITE
void AUX_indirect_dir_management (DirectoryHandle* d, int mode) {
	
	// Preliminary stuffs
	if (d == NULL) return;
	DiskDriver* disk = d->infs->disk;
	int snorlax = TBA;
	iNode_indirect* aux_node = (iNode_indirect*) malloc(sizeof(iNode_indirect));
	
	// Case 1 : we're in the main node (type DIR), so f->indirect == NULL;
	// if there's need to create an indirect node, create it, write it and locate the dirhandle
	if (d->indirect == NULL) {
		// There's space in DIR
		if (d->pos_in_node < inode_idx_size) return;
		// Need to create indirect node
		if (d->dcb->single_indirect == TBA && mode == WRITE) {
			
			int voyager = DiskDriver_getFreeBlock(disk, 0);
			if (voyager == TBA) {
				printf ("ERROR DISK FULL - @ AUX_indirect_dir_management()\n");
				return;
			}
			
			// Creating BlockHeader
			BlockHeader header;
			header.block_in_file = TBA;
			header.block_in_node = SINGLE;
			header.block_in_disk = voyager;
			
			// Creating icb
			iNodeControlBlock icb;
			if (d->directory != NULL) {
				icb.directory_block = d->directory->header.block_in_disk;
			} else icb.directory_block = TBA;
			icb.block_in_disk = header.block_in_disk;
			icb.upper = d->dcb->header.block_in_disk;
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
				printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
				
				// Freeing memory
				aux_node = NULL;
				free (aux_node);
				return;
			}
			// Updating d->dcb
			d->dcb->single_indirect = aux_node->header.block_in_disk;
			d->dcb->fcb.size_in_blocks += 1;
			d->dcb->fcb.size_in_bytes += BLOCK_SIZE;
			snorlax = DiskDriver_writeBlock(disk, d->dcb, d->dcb->header.block_in_disk);
			if (snorlax == TBA) {
				printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
				
				// Freeing memory
				aux_node = NULL;
				free (aux_node);
				return;
			}
			
			// Updating d
			d->indirect = aux_node;
			d->current_block = &(aux_node->header);
			d->pos_in_node = 0;
			d->pos_in_block = 0;
			
		}
		// Don't need to create indiecto node : it already exists
		// just update d
		else if (d->dcb->single_indirect != TBA) {
			// Updating d
			snorlax = DiskDriver_readBlock(disk, aux_node, d->dcb->single_indirect);
			if (snorlax == TBA) {
				printf ("ERROR READING - @ AUX_indirect_dir_management()\n");
				
				// Freeing memory 
				aux_node = NULL;
				free (aux_node);
				return;
			}
			d->indirect = aux_node;
			d->current_block = &(aux_node->header);
			d->pos_in_node = 0;
			d->pos_in_block = 0;
		}
	}
	// We're in the single indirect
	else {
		if (d->indirect->header.block_in_disk == d->dcb->single_indirect && 
			d->indirect->icb.upper == d->dcb->header.block_in_disk) {
				
			if (d->pos_in_node < indirect_idx_size) return;
			else {
				// double indirect also exists: just move there d
				if (d->dcb->double_indirect != TBA) {
					// Updating d
					snorlax = DiskDriver_readBlock(disk, aux_node, d->dcb->double_indirect);
					if (snorlax == TBA) {
						printf ("ERROR READING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					d->indirect = aux_node;
					d->current_block = &(aux_node->header);
					d->pos_in_node = 0;
					d->pos_in_block = 0;
				}
				// double indirect does not exists: create it and move there d
				else if (d->dcb->double_indirect == TBA && mode == WRITE) {
					memset(aux_node, 0, BLOCK_SIZE);
					int voyager = DiskDriver_getFreeBlock(disk, 0);
					if (voyager == TBA) {
						printf ("ERROR DISK FULL - @ AUX_indirect_dir_management()\n");
						return;
					}
					
					// Creating BlockHeader
					BlockHeader header;
					header.block_in_file = TBA;
					header.block_in_node = DOUBLE;
					header.block_in_disk = voyager;
					
					// Creating icb
					iNodeControlBlock icb;
					if (d->directory != NULL) {
						icb.directory_block = d->directory->header.block_in_disk;
					} else icb.directory_block = TBA;
					icb.block_in_disk = header.block_in_disk;
					icb.upper = d->dcb->header.block_in_disk;
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
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating d->dcb
					d->dcb->double_indirect = aux_node->header.block_in_disk;
					d->dcb->fcb.size_in_blocks += 1;
					d->dcb->fcb.size_in_bytes += BLOCK_SIZE;
					snorlax = DiskDriver_writeBlock(disk, d->dcb, d->dcb->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating d
					d->indirect = aux_node;
					d->current_block = &(aux_node->header);
					d->pos_in_node = 0;
					d->pos_in_block = 0;
				}
			}
		}
		// we are in the double indirect
		else if (d->indirect->header.block_in_disk == d->dcb->double_indirect &&
				d->indirect->icb.upper == d->dcb->header.block_in_disk) {
		
			if (d->pos_in_node < indirect_idx_size) {
				// Need to create another NOD, then update d
				if (d->indirect->file_blocks[d->pos_in_block] == TBA && mode == WRITE) {
					memset(aux_node, 0, BLOCK_SIZE);
					int voyager = DiskDriver_getFreeBlock(disk, 0);
					if (voyager == TBA) {
						printf ("ERROR DISK FULL - @ AUX_indirect_dir_management()\n");
						return;
					}
					
					// Creating BlockHeader
					BlockHeader header;
					header.block_in_file = TBA;
					header.block_in_node = d->pos_in_node;
					header.block_in_disk = voyager;
					
					// Creating icb
					iNodeControlBlock icb;
					if (d->directory != NULL) {
						icb.directory_block = d->directory->header.block_in_disk;
					} else icb.directory_block = TBA;
					icb.block_in_disk = header.block_in_disk;
					icb.upper = d->indirect->header.block_in_disk;
					icb.node_type = NOD;
					
					// NOD Stuffs
					aux_node->header = header;
					aux_node->icb = icb;
					aux_node->num_entries = 0;
					for (int i = 0; i < indirect_idx_size; ++i) {
						aux_node->file_blocks[i] = TBA;
					}
					
					// Writing on the disk
					snorlax = DiskDriver_writeBlock(disk, aux_node, aux_node->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating d->indirect
					d->indirect->file_blocks[d->pos_in_block] = aux_node->header.block_in_disk;
					snorlax = DiskDriver_writeBlock(disk, d->indirect, d->indirect->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating d->dcb
					d->dcb->fcb.size_in_blocks += 1;
					d->dcb->fcb.size_in_bytes += BLOCK_SIZE;
					snorlax = DiskDriver_writeBlock(disk, d->dcb, d->dcb->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating d
					d->indirect = aux_node;
					d->current_block = &(aux_node->header);
					d->pos_in_node = 0;
					d->pos_in_block = 0;
				}
				// double indirect's NOD also exists. Just Update d
				else {
					snorlax = DiskDriver_readBlock(disk, aux_node, d->indirect->file_blocks[d->pos_in_block]);
					if (snorlax == TBA) {
						printf ("ERROR READING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating d
					d->indirect = aux_node;
					d->current_block = &(aux_node->header);
					d->pos_in_node = 0;
					d->pos_in_block = 0;
				}
			}
			// NO MORE SPACE
			else {
				printf ("TOO LARGE DIR - @ AUX_indirect_dir_management()\n");
				return;
			}
		}
		// We are in a double indirect NOD
		else if (d->indirect->icb.upper == d->dcb->double_indirect &&
				d->indirect->header.block_in_disk != d->dcb->double_indirect) {
			// Check if it's full. if so, move to next one
			// If the next one does not exists, create and move
			if (d->pos_in_node < indirect_idx_size) return;
			else {
				// Read the parent node.
				snorlax = DiskDriver_readBlock(disk, aux_node, d->indirect->icb.upper);
				if (snorlax == TBA) {
					printf ("ERROR READING - @ AUX_indirect_dir_management()\n");
					
					// Freeing memory)
					aux_node = NULL;
					free (aux_node);
					return;
				}
				// check if the next NOD exists.
				// if exists, move					
				if (aux_node->file_blocks[d->indirect->header.block_in_node+1] != TBA) {
					snorlax = DiskDriver_readBlock(disk, aux_node, aux_node->file_blocks[d->indirect->header.block_in_node+1]);
					if (snorlax == TBA) {
					printf ("ERROR READING - @ AUX_indirect_dir_management()\n");
					
					// Freeing memory)
					aux_node = NULL;
					free (aux_node);
					return;
					}
					
					d->indirect = aux_node;
					d->current_block = &(aux_node->header);
					d->pos_in_node = 0;
					d->pos_in_block = 0;					
				}
				// else if it does not exists AND mod == WRITE, create and move
				else if (aux_node->file_blocks[d->indirect->header.block_in_node+1] == TBA &&
						mode == WRITE) {
					iNode_indirect* another_node = (iNode_indirect*) malloc(sizeof(iNode_indirect));
					memset(another_node, 0, BLOCK_SIZE);
					int voyager = DiskDriver_getFreeBlock(disk, 0);
					if (voyager == TBA) {
						printf ("ERROR DISK FULL - @ AUX_indirect_dir_management()\n");
						return;
					}
					
					// Creating blockheader
					BlockHeader header;
					header.block_in_file = TBA;
					header.block_in_node = d->indirect->header.block_in_node+1;
					header.block_in_disk = voyager;
					
					// Creating icb
					iNodeControlBlock icb;
					if (d->directory != NULL) {
						icb.directory_block = d->directory->header.block_in_disk;
					} else icb.directory_block = TBA;
					icb.block_in_disk = header.block_in_disk;
					icb.upper = aux_node->header.block_in_disk;
					icb.node_type = NOD;
					
					// NOD stuffs
					another_node->header = header;
					another_node->icb = icb;
					another_node->num_entries = 0;
					for (int i = 0; i < indirect_idx_size; ++i) {
						another_node->file_blocks[i] = TBA;
					}
					
					// Writing on the disk
					snorlax = DiskDriver_writeBlock(disk, another_node, another_node->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						another_node = NULL;
						free (another_node);
						return;
					}
					
					// Updating aux_node
					aux_node->file_blocks[another_node->header.block_in_node] = another_node->header.block_in_disk;
					snorlax = DiskDriver_writeBlock(disk, aux_node, aux_node->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						another_node = NULL;
						free (another_node);
						return;
					}
					
					// Update d->dcb sizes
					d->dcb->fcb.size_in_blocks += 1;
					d->dcb->fcb.size_in_bytes += BLOCK_SIZE;
					snorlax = DiskDriver_writeBlock(disk, d->dcb, d->dcb->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						another_node = NULL;
						free (another_node);
						return;
					}
					
					// Updating d
					d->indirect = another_node;
					d->current_block = &(another_node->header);
					d->pos_in_node = 0;
					d->pos_in_block = 0;
					
					// Freeing aux_node
					aux_node = NULL;
					free (aux_node);
				}
			}
		}
	}
	
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
	int filecount = 0;
	
	// Creating a copy of the dirhandle and searching for an already existent file
	// daux is set to first block of main inode to start the search
	// first_free_occurrency will be lately set as a duplicate of daux
	int first_free_flag = TBA;
	DirectoryHandle* daux = AUX_duplicate_dirhandle(d);
	daux->indirect = NULL;
	daux->current_block = &(daux->dcb->header);
	daux->pos_in_node = 0;
	daux->pos_in_block = 0;
	DirectoryHandle* first_free_occurrency;
	
	// Search in the main inode
	for (int i = 0; i < inode_idx_size; ++i) {
		if (daux->dcb->file_blocks[i] == TBA) {
			// Update the flag and save the first free occurrence
			if (first_free_flag == TBA) {
				first_free_flag = 0;
				first_free_occurrency = AUX_duplicate_dirhandle(daux);
			}
		}
		// Reading the node to check for equal file name
		// if snorlax == TBA, the block is free according to the bitmap
		// else check the node name
		else {
			++filecount;
			snorlax = DiskDriver_readBlock(disk, aux_node, daux->dcb->file_blocks[i]);
			if (snorlax != TBA) {
				if (aux_node->fcb.icb.node_type == FIL && strcmp(aux_node->fcb.name, filename) == 0) {
					printf ("FILE %s ALREADY EXISTS. CREATION FAILED @ iNodeFS_createFile()\n", filename);
								
					// Freeing memory
					aux_node = NULL;
					free (aux_node);		
					daux = NULL;
					free(daux);
					first_free_occurrency = NULL;
					free (first_free_occurrency);
		
					return NULL;
				}
				daux->current_block = &(aux_node->header);
			}
		}
		++daux->pos_in_node;
	}
	// Manager call
	// Check if we are in the single indirect
	if (filecount == daux->pos_in_node) {
		AUX_indirect_dir_management(daux, WRITE);
	} else {
		AUX_indirect_dir_management(daux, READ);
	}

	filecount = 0;
	if (daux->indirect != NULL && daux->indirect->header.block_in_disk == daux->dcb->single_indirect) {
		for (int i = 0; i < indirect_idx_size; ++i) {
			if (daux->indirect->file_blocks[i] == TBA) {
				// Update the flag and save the first free occurrence
				if (first_free_flag == TBA) {
					first_free_flag = 0;
					first_free_occurrency = AUX_duplicate_dirhandle(daux);
				}
			}
			// Reading the node to check for equal file name
			else {
				++filecount;
				snorlax = DiskDriver_readBlock(disk, aux_node, daux->indirect->file_blocks[i]);
				if (snorlax != TBA) {
					if (aux_node->fcb.icb.node_type == FIL && strcmp(aux_node->fcb.name, filename) == 0){
						printf ("FILE %s ALREADY EXISTS. CREATION FAILED @ iNodeFS_createFile()\n", filename);
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						daux = NULL;
						free (daux);
						first_free_occurrency = NULL;
						free (first_free_occurrency);
						
						return NULL;
					}
					daux->current_block = &(aux_node->header);
				}
			}
			++daux->pos_in_node;
		}
	}

	// Manager call
	// Check if we are in the single indirect
	if (filecount == daux->pos_in_node) {
		AUX_indirect_dir_management(daux, WRITE);
	} else {
		AUX_indirect_dir_management(daux, READ);
	}
/*	
	printf ("daux pos in node: %d\n", daux->pos_in_node);
	printf ("daux pos in block: %d\n", daux->pos_in_node);
	if (daux->indirect != NULL) printf ("daux indirect: %d\n", daux->indirect->icb.block_in_disk);
	else printf ("daux is on the main node\n");
	printf ("daux current block: %d\n", daux->current_block->block_in_disk);
		printf ("FFO pos in node: %d\n", first_free_occurrency->pos_in_node);
	printf ("FFO pos in block: %d\n", first_free_occurrency->pos_in_node);
	if (daux->indirect != NULL) printf ("FFO indirect: %d\n", first_free_occurrency->indirect->icb.block_in_disk);
	else printf ("FFO is on the main node\n");
	printf ("FFO current block: %d\n\n", first_free_occurrency->current_block->block_in_disk);
*/	
	
	if (daux->indirect != NULL && daux->indirect->header.block_in_disk == daux->dcb->double_indirect) {
		
		int upper_pos = daux->indirect->header.block_in_disk;
		
		if (daux->indirect->file_blocks[daux->pos_in_node] == TBA) {
			AUX_indirect_dir_management(daux, WRITE);
		} else {
			AUX_indirect_dir_management(daux, READ);
		}
		
		iNode_indirect* upper = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		snorlax = DiskDriver_readBlock(disk, upper, upper_pos);
		if (snorlax == TBA) {
			printf ("ERROR READING @ iNodeFS_createFile()\n");
			
			// Freeing memory
			aux_node = NULL;
			free (aux_node);
			daux = NULL;
			free (daux);
			first_free_occurrency = NULL;
			free (first_free_occurrency);
			upper = NULL;
			free (upper);
			
			return NULL;
		}
		
		// Now daux is on a NOD. The behavior shold be the same as in a single_indirect:
		// Calling the manager at this point will automatically create another NOD if needed
		for (int i = 0; i < indirect_idx_size; ++i) {
			
			if (upper->file_blocks[i] != TBA) {
				filecount = 0;
				if (daux->indirect != NULL && daux->indirect->icb.upper == daux->dcb->double_indirect) {
					for (int i = 0; i < indirect_idx_size; ++i) {
						if (daux->indirect->file_blocks[i] == TBA) {
							// Update flag and save first free occurrency
							if (first_free_flag == TBA) {
								first_free_flag = 0;
								first_free_occurrency = AUX_duplicate_dirhandle(daux);
							}
						}
						// Reading the node to check for equal file name
						else {
							++filecount;
							snorlax = DiskDriver_readBlock(disk, aux_node, daux->indirect->file_blocks[i]);
							if (snorlax != TBA) {
								if (aux_node->fcb.icb.node_type == FIL && strcmp(aux_node->fcb.name, filename) == 0) {
									printf ("FILE %s ALREADY EXISTS. CREATION FAILED @ iNodeFS_createFile()\n", filename);
						
									// Freeing memory
									aux_node = NULL;
									free (aux_node);
									daux = NULL;
									free (daux);
									first_free_occurrency = NULL;
									free (first_free_occurrency);
									upper = NULL;
									free (upper);
									
									return NULL;
								}
								daux->current_block = &(aux_node->header);
							}
						}
						++daux->pos_in_node;
					}
				}
				if (filecount == daux->pos_in_node) {
					AUX_indirect_dir_management(daux, WRITE);
				} else {
					AUX_indirect_dir_management(daux, READ);
				}
				snorlax = DiskDriver_readBlock(disk, upper, upper->header.block_in_disk);
				if (snorlax == TBA) {			
					printf ("ERROR READING - @ iNodeFS_mkdir()\n");
							
					// Freeing memory
					aux_node = NULL;
					free (aux_node);		
					daux = NULL;
					free(daux);
					first_free_occurrency = NULL;
					free (first_free_occurrency);
					upper = NULL;
					free (upper);

					return NULL;
				} 
			}
			else {
				upper = NULL;
				free (upper);
				break;
			}
			
		}
		
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
	
	/*** Must work on free_first_occurrency ***/
	
	// Updating dirhandle
	// We are in the main inode
	if (first_free_occurrency->indirect == NULL) {
		d->dcb->file_blocks[first_free_occurrency->pos_in_node] = voyager;
	}
	// Need another update
	else {
		first_free_occurrency->indirect->file_blocks[first_free_occurrency->pos_in_node] = voyager;
		++first_free_occurrency->indirect->num_entries;
		snorlax = DiskDriver_writeBlock(disk, first_free_occurrency->indirect, first_free_occurrency->indirect->header.block_in_disk);
		if (snorlax == TBA) {
			printf ("ERROR UPDATING DCB ON THE DISK @ iNodeFS_createFile()\n");
			
			return NULL;
		}		
	}
	d->dcb->num_entries += 1;
	snorlax = DiskDriver_writeBlock(disk, d->dcb, d->dcb->header.block_in_disk);
	if (snorlax == TBA) {
		printf ("ERROR UPDATING DCB ON THE DISK @ iNodeFS_createFile()\n");
		return NULL;
	}
	d = daux;
	
	// Freeing memory	
	daux = NULL;
	free(daux);
	first_free_occurrency = NULL;
	free (first_free_occurrency);
	
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
	
	// Single Indirect
	if (d->dcb->single_indirect != TBA) {
		iNode_indirect* single_indirect = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		snorlax = DiskDriver_readBlock(disk, single_indirect, d->dcb->single_indirect);
		if (snorlax == TBA) {
			printf ("ERROR READING @ iNodeFS_readDir()\n");
			
			// Freeing Memoy
			aux_node = NULL;
			free (aux_node);
			single_indirect = NULL;
			free (single_indirect);
			return TBA;
		}
		for (int i = 0; i < indirect_idx_size; ++i) {
			if (single_indirect->file_blocks[i] != TBA) {
				snorlax = DiskDriver_readBlock(disk, aux_node, single_indirect->file_blocks[i]);
				if (snorlax != TBA) {
					strcpy(names[j], aux_node->fcb.name);
					++j;
				}
			}
		}
		// Freeing memory
		single_indirect = NULL;
		free (single_indirect);
	}
	
	// Double Indirect
	if (d->dcb->double_indirect != TBA) {
		iNode_indirect* double_indirect = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		snorlax = DiskDriver_readBlock(disk, double_indirect, d->dcb->double_indirect);
		if (snorlax == TBA) {
			printf ("ERROR READING @ iNodeFS_readDir()\n");
			
			// Freeing memory
			aux_node = NULL;
			free (aux_node);
			double_indirect = NULL;
			free (double_indirect);
			return TBA;
		}
		iNode_indirect* nod = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		for (int i = 0; i < indirect_idx_size; ++i) {
			if (double_indirect->file_blocks[i] != TBA) {
				snorlax = DiskDriver_readBlock(disk, nod, double_indirect->file_blocks[i]);
				if (snorlax == TBA) {
					printf ("ERROR READING @ iNodeFS_readDir()\n");
			
					// Freeing memory
					aux_node = NULL;
					free (aux_node);
					double_indirect = NULL;
					free (double_indirect);
					nod = NULL;
					free (nod);
					return TBA;
				}
				for (int k = 0; k < indirect_idx_size; ++k) {
					if (nod->file_blocks[k] != TBA) {
						snorlax = DiskDriver_readBlock(disk, aux_node, nod->file_blocks[k]);
						if (snorlax != TBA) {
							strcpy(names[j], aux_node->fcb.name);
							++j;
						}
					}
				}
			}
		}
		// Freeing memory
		nod = NULL;
		free (nod);
		double_indirect = NULL;
		free (double_indirect);	
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
	
	// Single Indirect
	if (d->dcb->single_indirect != TBA) {
		iNode_indirect* single_indirect = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		snorlax = DiskDriver_readBlock(disk, single_indirect, d->dcb->single_indirect);
		if (snorlax == TBA) {
			printf ("ERROR READING @ iNodeFS_openFile()\n");
			
			// Freeing Memoy
			filehandle = NULL;
			free (filehandle);
			aux_node = NULL;
			free (aux_node);
			single_indirect = NULL;
			free (single_indirect);
			return NULL;
		}
		for (int i = 0; i < indirect_idx_size; ++i) {
			if (single_indirect->file_blocks[i] != TBA) {
				snorlax = DiskDriver_readBlock(disk, aux_node, single_indirect->file_blocks[i]);
				if (snorlax != TBA) {
					if (strcmp(aux_node->fcb.name, filename) == 0 && aux_node->fcb.icb.node_type == FIL){
						filehandle->fcb = aux_node;
						filehandle->current_block = &(aux_node->header);
						
						// Freeing memory
						single_indirect = NULL;
						free (single_indirect);
						
						return filehandle;
					}
				}
			}
		}
		// Freeing memory
		single_indirect = NULL;
		free (single_indirect);
	}
	
	// Double Indirect
	if (d->dcb->double_indirect != TBA) {
		iNode_indirect* double_indirect = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		snorlax = DiskDriver_readBlock(disk, double_indirect, d->dcb->double_indirect);
		if (snorlax == TBA) {
			printf ("ERROR READING @ iNodeFS_openFile()\n");
			
			// Freeing memory
			aux_node = NULL;
			free (aux_node);
			double_indirect = NULL;
			free (double_indirect);
			return NULL;
		}
		iNode_indirect* nod = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		for (int i = 0; i < indirect_idx_size; ++i) {
			if (double_indirect->file_blocks[i] != TBA) {
				snorlax = DiskDriver_readBlock(disk, nod, double_indirect->file_blocks[i]);
				if (snorlax == TBA) {
					printf ("ERROR READING @ iNodeFS_openFile()\n");
			
					// Freeing memory
					aux_node = NULL;
					free (aux_node);
					double_indirect = NULL;
					free (double_indirect);
					nod = NULL;
					free (nod);
					return NULL;
				}
				for (int j = 0; j < indirect_idx_size; ++j) {
					if (nod->file_blocks[j] != TBA) {
						snorlax = DiskDriver_readBlock(disk, aux_node, nod->file_blocks[j]);
						if (snorlax != TBA) {
							if (strcmp(aux_node->fcb.name, filename) == 0 && aux_node->fcb.icb.node_type == FIL) {
								filehandle->fcb = aux_node;
								filehandle->current_block = &(aux_node->header);
								
								// Freeing memory
								double_indirect = NULL;
								free (double_indirect);
								nod = NULL;
								free (nod);
								
								return filehandle;
							}
						}
					}
				}
			}
		}
		// Freeing memory
		double_indirect = NULL;
		free (double_indirect);
		nod = NULL;
		free (nod);
		return NULL;
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


// puts the filehandle at the right place in the inode with side effect on the filehandle
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
				return;
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
						return;
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
						return;
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
					
					// Updating f->indirect
					f->indirect->file_blocks[f->pos_in_block] = aux_node->header.block_in_disk;
					snorlax = DiskDriver_writeBlock(disk, f->indirect, f->indirect->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						return;
					}
					
					// Updating f->fcb
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
		// We are in a double indirect NOD
		else if (f->indirect->icb.upper == f->fcb->double_indirect && 
				f->indirect->header.block_in_disk != f->fcb->double_indirect) {
			// If the next one does not exists, create and move
			if (f->pos_in_node < indirect_idx_size) return;
			else {
				// Read the parent node
				snorlax = DiskDriver_readBlock(disk, aux_node, f->indirect->icb.upper);
				if (snorlax == TBA) {
					printf ("ERROR READING @ AUX_indirect_management()\n");
					
					// Freeing memory
					aux_node = NULL;
					free (aux_node);
					return;
				}
				// Check if the next NOD exists.
				// if exists, move
				if (aux_node->file_blocks[f->indirect->header.block_in_node+1] != TBA) {
					snorlax = DiskDriver_readBlock(disk, aux_node, aux_node->file_blocks[f->indirect->header.block_in_node+1]);
					if (snorlax == TBA) {
						printf ("ERROR READING @ AUX_indirect_management()\n");
					
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
				// else if it does not exists AND mode == WRITE, create and move
				else if (aux_node->file_blocks[f->indirect->header.block_in_node+1] == TBA && 
						mode ==	WRITE) {
					iNode_indirect* another_node = (iNode_indirect*) malloc(sizeof(iNode_indirect));
					memset(another_node, 0, BLOCK_SIZE);
					int voyager = DiskDriver_getFreeBlock(disk, 0);
					if (voyager == TBA) {
						printf ("ERROR DISK FULL @ AUX_indirect_management()\n");
						return;
					}
					
					// Creating blockheader
					BlockHeader header;
					header.block_in_file = TBA;
					header.block_in_node = f->indirect->header.block_in_node+1;
					header.block_in_disk = voyager;
					
					// Creating icb
					iNodeControlBlock icb;
					if (f->directory != NULL) {
						icb.directory_block = f->directory->header.block_in_disk;
					} else icb.directory_block = TBA;
					icb.block_in_disk = header.block_in_disk;
					icb.upper = aux_node->header.block_in_disk;
					icb.node_type = NOD;
					
					// NOD stuffs
					another_node->header = header;
					another_node->icb = icb;
					another_node->num_entries = 0;
					for (int i = 0; i < indirect_idx_size; ++i) {
						another_node->file_blocks[i] = TBA;
					}
					
					// Writing on the disk
					snorlax = DiskDriver_writeBlock(disk, another_node, another_node->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						another_node = NULL;
						free (another_node);
						return;
					}
					
					// Updating aux_node
					aux_node->file_blocks[another_node->header.block_in_node] = another_node->header.block_in_disk;
					snorlax = DiskDriver_writeBlock(disk, aux_node, aux_node->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						another_node = NULL;
						free (another_node);
						return;
					}
					
					// Update d->dcb sizes
					f->fcb->fcb.size_in_blocks += 1;
					f->fcb->fcb.size_in_bytes += BLOCK_SIZE;
					snorlax = DiskDriver_writeBlock(disk, f->fcb, f->fcb->header.block_in_disk);
					if (snorlax == TBA) {
						printf ("ERROR WRITING - @ AUX_indirect_dir_management()\n");
						
						// Freeing memory
						aux_node = NULL;
						free (aux_node);
						another_node = NULL;
						free (another_node);
						return;
					}
					
					// Updating d
					f->indirect = another_node;
					f->current_block = &(another_node->header);
					f->pos_in_node = 0;
					f->pos_in_block = 0;
					
					// Freeing aux_node
					aux_node = NULL;
					free (aux_node);
					
				}
			}
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
				/*** THIS WRITE COULD BE USELESS ***/
				// The update is on the write just before this, so this is useless
				// also check at the same point for single indirect
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
			d->dcb = aux_node;
			d->current_block = &(aux_node->header);
			if (d->dcb->fcb.icb.directory_block != TBA) {
				snorlax = DiskDriver_readBlock(disk, d->directory, d->dcb->fcb.icb.directory_block);
			} else d->directory = NULL;
			
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
					d->directory = NULL;
					free (d->directory);
					d->directory = d->dcb;
					d->dcb = aux_node;
					d->current_block = &(aux_node->header);
										
					return 0;
				}
			}
		}					
	}
	
	// Single Indirect
	if (d->dcb->single_indirect != TBA) {
		iNode_indirect* single_indirect = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		snorlax = DiskDriver_readBlock(disk, single_indirect, d->dcb->single_indirect);
		if (snorlax == TBA) {
			printf ("ERROR READING @ iNodeFS_changeDir()\n");
			
			// Freeing memory
			aux_node = NULL;
			free (aux_node);
			single_indirect = NULL;
			free (single_indirect);
			return TBA;
		}
		for (int i = 0; i < indirect_idx_size; ++i) {
			if (single_indirect->file_blocks[i] != TBA) {
				snorlax = DiskDriver_readBlock(disk, aux_node, single_indirect->file_blocks[i]);
				if (snorlax != TBA) {
					if (strcmp(aux_node->fcb.name, dirname) == 0 && aux_node->fcb.icb.node_type == DIR) {
						d->directory = NULL;
						free (d->directory);
						d->directory = d->dcb;
						d->dcb = aux_node;
						d->current_block = &(aux_node->header);
						
						// Freeing memory
						single_indirect = NULL;
						free (single_indirect);
						
						return 0;
					}
				}
			}
		}
		// Freeing memory
		single_indirect = NULL;
		free (single_indirect);
	}
	
	// Double Indirect
	if (d->dcb->double_indirect != TBA) {
		iNode_indirect* double_indirect = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		snorlax = DiskDriver_readBlock(disk, double_indirect, d->dcb->double_indirect);
		if (snorlax == TBA) {
			printf ("ERROR READING @ iNodeFS_changeDir()\n");
			
			// Freeing memory
			aux_node = NULL;
			free (aux_node);
			double_indirect = NULL;
			free (double_indirect);
			return TBA;
		}
		iNode_indirect* nod = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		for (int i = 0; i < indirect_idx_size; ++i) {
			if (double_indirect->file_blocks[i] != TBA) {
				snorlax = DiskDriver_readBlock(disk, nod, double_indirect->file_blocks[i]);
				if (snorlax == TBA) {
					printf ("ERROR READING @ iNodeFS_changeDir()\n");
			
					// Freeing memory
					aux_node = NULL;
					free (aux_node);
					double_indirect = NULL;
					free (double_indirect);
					nod = NULL;
					free (nod);
					return TBA;
				}
				for (int j = 0; j < indirect_idx_size; ++j) {
					if (nod->file_blocks[j] != TBA) {
						snorlax = DiskDriver_readBlock(disk, aux_node, nod->file_blocks[j]);
						if (snorlax != TBA) {
							if (strcmp(aux_node->fcb.name, dirname) == 0 && aux_node->fcb.icb.node_type == DIR) {
								d->directory = NULL;
								free (d->directory);
								d->directory = d->dcb;
								d->dcb = aux_node;
								d->current_block = &(aux_node->header);
								
								
								// Freeing memory
								double_indirect = NULL;
								free (double_indirect);
								nod = NULL;
								free (nod);
								return 0;
							}
						}						
					}
				}
			}
		}
		// Freeing memory
		nod = NULL;
		free (nod);
		double_indirect = NULL;
		free (double_indirect);		
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
	int filecount = 0;
	
	// Creating a copy of the dirhandle and searching for an already existent file
	// daux is set to first block of main inode to start the search
	// first_free_occurrency will be lately set as a duplicate of daux
	int first_free_flag = TBA;
	DirectoryHandle* daux = AUX_duplicate_dirhandle(d);
	daux->indirect = NULL;
	daux->current_block = &(daux->dcb->header);
	daux->pos_in_node = 0;
	daux->pos_in_block = 0;
	DirectoryHandle* first_free_occurrency;
	
	// Search in the inode
	for (int i = 0; i < inode_idx_size; ++i) {
		if (d->dcb->file_blocks[i] == TBA) {
			// Update the flag and save the first free occurrence
			if (first_free_flag == TBA) {
				first_free_flag = 0;
				first_free_occurrency = AUX_duplicate_dirhandle(daux);
			}
		}
		// Reading the node to check for equal file name
		// if snorlax == TBA, the block is free according to the bitmap
		// else check the node name
		else {
			++filecount;
			snorlax = DiskDriver_readBlock(disk, aux_node, daux->dcb->file_blocks[i]);
			if (snorlax != TBA) {
				if (aux_node->fcb.icb.node_type == DIR && strcmp(aux_node->fcb.name, dirname) == 0) {
					printf ("DIR %s ALREADY EXISTS. CREATION FAILED @ iNodeFS_mkdir()\n", dirname);
								
					// Freeing memory
					aux_node = NULL;
					free (aux_node);		
					daux = NULL;
					free(daux);
					first_free_occurrency = NULL;
					free (first_free_occurrency);
		
					return TBA;
				}
				daux->current_block = &(aux_node->header);
			}
		}
		++daux->pos_in_node;		
	}

	// Manager call
	// Check if we are in the single indirect
	if (filecount == daux->pos_in_node) {
		AUX_indirect_dir_management(daux, WRITE);
	} else {
		AUX_indirect_dir_management(daux, READ);
	}
	
	filecount = 0;
	if (daux->indirect != NULL && daux->indirect->header.block_in_disk == daux->dcb->single_indirect) {
		for (int i = 0; i < indirect_idx_size; ++i) {
			if (daux->indirect->file_blocks[i] == TBA) {
				// Update flag and save first free occurrence
				if (first_free_flag == TBA) {
					first_free_flag = 0;
					first_free_occurrency = AUX_duplicate_dirhandle(daux);
				}
			}
			// Reading the node to check for equal file name
			else {
				++filecount;
				snorlax = DiskDriver_readBlock(disk, aux_node, daux->indirect->file_blocks[i]);
				if (snorlax != TBA) {
					if (aux_node->fcb.icb.node_type == DIR && strcmp(aux_node->fcb.name, dirname) == 0){
						printf ("DIR %s ALREADY EXISTS. CREATION FAILED @ iNodeFS_mkdir()\n", dirname);
								
						// Freeing memory
						aux_node = NULL;
						free (aux_node);		
						daux = NULL;
						free(daux);
						first_free_occurrency = NULL;
						free (first_free_occurrency);
			
						return TBA;
					}
					daux->current_block = &(aux_node->header);
				}
			}
			++daux->pos_in_node;
		}
	}
	
	// Manager call
	// Check if we are in the double indirect 
	if (filecount == daux->pos_in_node) {
		AUX_indirect_dir_management(daux, WRITE);
	} else {
		AUX_indirect_dir_management(daux, READ);
	}
	
	// Once here, just calla the manager to check if it's necessary to create a new NOD or not
	if (daux->indirect != NULL && daux->indirect->header.block_in_disk == daux->dcb->double_indirect) {
		
		int upper_pos = daux->indirect->header.block_in_disk;	
			
		if (daux->indirect->file_blocks[daux->pos_in_node] == TBA) {
			AUX_indirect_dir_management(daux, WRITE);
		} else {
			AUX_indirect_dir_management(daux, READ);
		}
		
		iNode_indirect* upper = (iNode_indirect*) malloc(sizeof(iNode_indirect));
		snorlax = DiskDriver_readBlock(disk, upper, upper_pos);
		if (snorlax == TBA) {			
			printf ("ERROR READING - @ iNodeFS_mkdir()\n");
					
			// Freeing memory
			aux_node = NULL;
			free (aux_node);		
			daux = NULL;
			free(daux);
			first_free_occurrency = NULL;
			free (first_free_occurrency);
			upper = NULL;
			free (upper);

			return TBA;
		}		
	
		// Now daux is on a NOD. The behavior shold be the same as in a single_indirect:
		// Calling the manager at this point will automatically create another NOD if needed
		 for (int i = 0; i < indirect_idx_size; ++i) {
			
			if (upper->file_blocks[i] != TBA) {
				filecount = 0;
				if (daux->indirect != NULL && daux->indirect->icb.upper == daux->dcb->double_indirect) {
					for (int i = 0; i < indirect_idx_size; ++i) {
						if (daux->indirect->file_blocks[i] == TBA) {
							// Update flag and save first free occurrence
							if (first_free_flag == TBA) {
								first_free_flag = 0;
								first_free_occurrency = AUX_duplicate_dirhandle(daux);
							}
						}
						// Reading the node to check for equal file name
						else {
							++filecount;
							snorlax = DiskDriver_readBlock(disk, aux_node, daux->indirect->file_blocks[i]);
							if (snorlax != TBA) {
								if (aux_node->fcb.icb.node_type == DIR && strcmp(aux_node->fcb.name, dirname) == 0) {
									printf ("DIR %s ALREADY EXISTS. CREATION FAILED @ iNodeFS_mkdir()\n", dirname);
									
									// Freeing memory
									aux_node = NULL;
									free (aux_node);
									daux = NULL;
									free (daux);
									first_free_occurrency = NULL;
									free (first_free_occurrency);
									upper = NULL;
									free (upper);
									
									return TBA;
								}
								daux->current_block = &(aux_node->header);
							}
						}
						++daux->pos_in_node;
					}
				}
				if (filecount == daux->pos_in_node) {
					AUX_indirect_dir_management(daux, WRITE);
				} else {
					AUX_indirect_dir_management(daux, READ);
				}
				snorlax = DiskDriver_readBlock(disk, upper, upper->header.block_in_disk);
				if (snorlax == TBA) {			
					printf ("ERROR READING - @ iNodeFS_mkdir()\n");
							
					// Freeing memory
					aux_node = NULL;
					free (aux_node);		
					daux = NULL;
					free(daux);
					first_free_occurrency = NULL;
					free (first_free_occurrency);
					upper = NULL;
					free (upper);

					return TBA;
				}				
			}
			else {
				upper = NULL;
				free (upper);
				break;
			}
		}
	}
	
/*		
	printf ("daux pos in node: %d\n", daux->pos_in_node);
	printf ("daux pos in block: %d\n", daux->pos_in_node);
	if (daux->indirect != NULL) printf ("daux indirect: %d\n", daux->indirect->icb.block_in_disk);
	else printf ("daux is on the main node\n");
	printf ("daux current block: %d\n", daux->current_block->block_in_disk);
	printf ("FFO pos in node: %d\n", first_free_occurrency->pos_in_node);
	printf ("FFO pos in block: %d\n", first_free_occurrency->pos_in_node);
	if (daux->indirect != NULL) printf ("FFO indirect: %d\n", first_free_occurrency->indirect->icb.block_in_disk);
	else printf ("FFO is on the main node\n");
	printf ("FFO current block: %d\n\n", first_free_occurrency->current_block->block_in_disk);
*/
	
	// Creation time
	memset(aux_node, 0, BLOCK_SIZE);
	int voyager = DiskDriver_getFreeBlock(disk, 0);
	if (voyager == TBA) {
		printf ("ERROR - DISK COULD BE FULL @ iNodeFS_mkdir()\n");
		
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
		printf ("ERROR WRITING AUX NODE ON THE DISK @ iNodeFS_mkdir()\n");
		
		// Freeing memory
		aux_node = NULL;
		free (aux_node);		
		daux = NULL;
		free(daux);
		
		return TBA;
	}
	
	// Updating dirhandle
	// We are in the main node
	if (first_free_occurrency->indirect == NULL) {
		d->dcb->file_blocks[first_free_occurrency->pos_in_node] = voyager;
	}
	// Need another update
	else {
		first_free_occurrency->indirect->file_blocks[first_free_occurrency->pos_in_node] = voyager;
		++first_free_occurrency->indirect->num_entries;
		snorlax = DiskDriver_writeBlock(disk, first_free_occurrency->indirect, first_free_occurrency->indirect->header.block_in_disk);
		if (snorlax == TBA) {
			printf ("ERROR UPDATING DCB ON THE DISK @ iNodeFS_mkdir()\n");
			return TBA;
		}
	}
	d->dcb->num_entries += 1;
	snorlax = DiskDriver_writeBlock(disk, d->dcb, d->dcb->header.block_in_disk);
	if (snorlax == TBA) {
		printf ("ERROR UPDATING DCB ON THE DISK @ iNodeFS_mkdir()\n");
			return TBA;
	}
	
	d = daux;
		
	// Freeing memory	
	daux = NULL;
	free(daux);
	first_free_occurrency = NULL;
	free (first_free_occurrency);
	
	return 0;
}

// removes the file in the current directory
// returns -1 on failure 0 on success
// if a directory, it removes recursively all contained files
int iNodeFS_remove(DirectoryHandle* d, char* filename);
