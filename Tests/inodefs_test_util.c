#include "inodefs_test_util.h"
#include <stdio.h>

// Prints the Disk Driver content
void iNodeFS_print (iNodeFS* fs, DirectoryHandle* d) {
	if (fs == NULL || d == NULL) return;
	printf ("-------- DISK DRIVER --------    iNodeFS_print()\n");
	DiskDriver* disk = fs->disk;
	printf ("Header\n");
	printf ("num_blocks	        : %d\n", disk->header->num_blocks);
	printf ("bitmap_blocks		: %d\n", disk->header->bitmap_blocks);
	printf ("bitmap_entries		: %d\n", disk->header->bitmap_entries);
	printf ("free_blocks		: %d\n", disk->header->free_blocks);
	printf ("first_free_block	: %d\n", disk->header->first_free_block);
	
	for (int i = 0; i < disk->header->bitmap_entries; ++i) {
		printf ("[ %d ] ", disk->bitmap_data[i]);
	}
	printf ("\n\n");
}

// Prints the given handle
void iNodeFS_printHandle (void* h) {
	printf ("------- iNodeFS_printHandle() \n");
	if (h == NULL) {
		printf ("GIVEN HANDLER IS NULL\n");
		return;
	}
	if (((DirectoryHandle*) h)->dcb->fcb.node_type == DIR) {
		DirectoryHandle* handle = (DirectoryHandle*) h;
		printf ("-- You are now working in \n");
		printf ("Directory             : %s\n", handle->dcb->fcb.name);
		printf ("Size in Blocks        : %d\n", handle->dcb->fcb.size_in_blocks);
		printf ("Size in Bytes         : %d\n", handle->dcb->fcb.size_in_bytes);
		printf ("Is Dir?               : %d\n", handle->dcb->fcb.node_type);
		printf ("Block in disk         : %d\n", handle->current_block->block_in_disk);
		printf ("Block in file         : %d\n", handle->current_block->block_in_file);
		if (handle->directory != NULL) 
			printf ("This dir's parent is  : %s\n", handle->directory->fcb.name);
		else printf ("This dir is root\n");
		printf ("Files in this folder  : %d\n", handle->dcb->num_entries);
		printf ("Position in node      : %d\n", handle->pos_in_node);
		printf ("Position in block     : %d\n", handle->pos_in_block);
	}
	if (((FileHandle*) h)->fcb->fcb.node_type == FIL) {
		FileHandle* handle = (FileHandle*) h;
		printf ("-- You are now working in \n");
		printf ("File                : %s\n", handle->fcb->fcb.name);
		printf ("Size in Blocks        : %d\n", handle->fcb->fcb.size_in_blocks);
		printf ("Size in Bytes         : %d\n", handle->fcb->fcb.size_in_bytes);
		printf ("Is Dir?               : %d\n", handle->fcb->fcb.node_type);
		printf ("Block in disk         : %d\n", handle->current_block->block_in_disk);
		printf ("Block in file         : %d\n", handle->current_block->block_in_file);
		printf ("Parent dir's block    : %d\n", handle->fcb->fcb.directory_block); 
		//printf ("Pos in file           : %d\n", handle->pos_in_file);
		printf ("Position in node      : %d\n", handle->pos_in_node);
		printf ("Position in block     : %d\n", handle->pos_in_block);
	}
}
