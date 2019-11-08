#include "inodefs_test_util.c"

int main (int argc, char** argv) {
	
	// * * * * FILE SYSTEM INITIALIZATION * * * *
	
	printf (BOLD_RED "\n* * * * FILE SYSTEM INITIALIZATION * * * *\n"COLOR_RESET);
	
	// Init the disk and the file system
	printf (YELLOW "\n\n**	Initializing Disk and File System - testing SimpleFS_init()\n\n" COLOR_RESET);
	
	DiskDriver disk;
	DiskDriver_init(&disk, "inodefs_test.txt", NUM_BLOCKS);
	
	iNodeFS fs;
	DirectoryHandle* dirhandle;
	FileHandle* filehandle;
	dirhandle = iNodeFS_init(&fs, &disk);
	
	if (dirhandle == NULL) {
		// Formatting the disk
		printf (YELLOW "\n\n**	Formatting File System - testing SimpleFS_format()\n\n" COLOR_RESET);
		iNodeFS_format(&fs);
		dirhandle = iNodeFS_init(&fs, &disk);
		iNodeFS_print(&fs, dirhandle);
	}
	
	iNodeFS_printHandle((void*)dirhandle);

	
	// * * * * TESTING BY SHELL * * * *
	
	if ( argc >= 2 && strcmp(argv[1], "shell") == 0) {
		
		printf (BOLD_RED "\n* * * * TESTING BY SHELL * * * *\n" COLOR_RESET);
		
		size_t len = MAX_CMD_LEN;
		
		char cmd1[MAX_CMD_LEN] = "";
		char cmd2[MAX_CMD_LEN] = "";
		char* line = NULL;
		
		int ret = -1;
		
		while (-TBA) {
			printf (BOLD_YELLOW "g@g:~%s " COLOR_RESET, dirhandle->dcb->fcb.name);
			
			getline(&line, &len, stdin);
			sscanf(line, "%s %s", cmd1, cmd2);
			
			// * * * GENERAL CMDs * * *
			if (strcmp(cmd1, SYS_SHOW) == 0 ){
				iNodeFS_print(&fs, dirhandle);
			}
			else if (strcmp(cmd1, SYS_HELP) == 0) {
				
				printf (YELLOW " GENERAL\n" COLOR_RESET
				SYS_SHOW"       : show status of File System\n"
				SYS_HELP"         : show list of commands\n"
				DIR_REMOVE" [obj]     : removes the object named 'obj'\n"
				YELLOW "\n DIR\n" COLOR_RESET
				DIR_SHOW"        : show actual directory info\n"
				DIR_CHANGE" [dir]     : goes into directory named 'dir'\n"
				DIR_MAKE" [dir]  : creates a directory named 'dir'\n"
				DIR_LS"           : prints the dir's content\n"
				YELLOW "\n FILE\n" COLOR_RESET
				FILE_SHOW"           : show the last opened file\n"
				FILE_MAKE" [fil]   : create a file named 'fil' \n"
				FILE_MAKE_N" [n]    : create n files\n"
				FILE_OPEN" [fil]    : open file named 'fil' \n"
				FILE_WRITE" [txt]   : writes 'txt' in the last opened file\n"
				FILE_READ"           : open the current file \n"
				FILE_SEEK" [n]      : moves the cursor at pos n in the opened file\n"
				
				FILE_CLOSE"        : closes the last opened file\n"
				
				);
			}
			
			
			// * * * DIRECTORIES CMDs * * *
			
			// print actual directory location
			else if (strcmp(cmd1, DIR_SHOW) == 0) {
				iNodeFS_printHandle(dirhandle);
			}
						
			// create a dir
			else if (strcmp(cmd1, DIR_MAKE) == 0) {
				ret = iNodeFS_mkDir(dirhandle, cmd2);
			}
		
			// create n dirs
			else if (strcmp(cmd1, DIR_MAKE_N) == 0) {
				printf (RED "WARNING : mknfil does not control the inserted number of files\n" COLOR_RESET);
				char dirnames[atoi(cmd2)][NAME_SIZE];
				for (int i = 0; i < atoi(cmd2); ++i) {
					gen_dirname(dirnames[i], i);
					ret = iNodeFS_mkDir(dirhandle, dirnames[i]);
				}
			}
			
			// Change directory
			else if (strcmp(cmd1, DIR_CHANGE) == 0) {
				ret = iNodeFS_changeDir(dirhandle, cmd2);
				if (ret == TBA) printf (RED "DIR '%s' DOES NOT EXIST\n" COLOR_RESET, cmd2);
			}

			
			// show dir content
			else if (strcmp(cmd1, DIR_LS) == 0) {
				char* names[NUM_BLOCKS];
				for (int i = 0; i < NUM_BLOCKS; ++i) {
					names[i] = (char*) calloc(NAME_SIZE, 0);
				}
				iNodeFS_readDir(names, dirhandle);
				iNodeFS_printArray(names, NUM_BLOCKS);
			}

/*			
			// delete a dir or a file
			else if (strcmp(cmd1, DIR_REMOVE) == 0) {
				ret = iNodeFS_remove(dirhandle, cmd2);
				if (ret == TBA) printf (RED "'%s' DOES NOT EXIST\n" COLOR_RESET, cmd2);
			}
*/			
			
			// * * * FILES CMDs * * *
			
			// print actual file proprieties
			else if (strcmp(cmd1, FILE_SHOW) == 0) {
				iNodeFS_printHandle(filehandle);
			}
			
			// Create a file
			else if (strcmp(cmd1, FILE_MAKE) == 0) {
				filehandle = iNodeFS_createFile(dirhandle, cmd2);
			}
			
			// Create n files
			else if (strcmp(cmd1, FILE_MAKE_N) == 0) {
				printf (RED "WARNING : mknfil does not control the inserted number of files\n" COLOR_RESET);
				char filenames[atoi(cmd2)][NAME_SIZE];
				for (int i = 0; i < atoi(cmd2); ++i) {
					gen_filename(filenames[i], i);
					filehandle = iNodeFS_createFile(dirhandle, filenames[i]);
				}
			}
			
			// open a file
			else if (strcmp(cmd1, FILE_OPEN) == 0) {
				filehandle = iNodeFS_openFile(dirhandle, cmd2);
			}
			
			// write a file
			else if (strcmp(cmd1, FILE_WRITE) == 0) {
				int c = 0;
				while (cmd2[c] != '\0') ++c;
				char bug[c];
				for (int i = 0; i < c; ++i) bug[i] = 0;
				strncpy(bug, cmd2, c);
				ret = iNodeFS_write(filehandle, bug, sizeof(bug));
				printf ("written bytes :  %d\n", ret);
			}
		
			// seek
			else if (strcmp(cmd1, FILE_SEEK) == 0) {
				ret = iNodeFS_seek(filehandle, atoi(cmd2));
			}
			
			// read a file
			else if (strcmp(cmd1, FILE_READ) == 0 && filehandle != NULL) {
				int cmd_len = 128;
				char text[cmd_len];
				ret = iNodeFS_read(filehandle, text, cmd_len);
				printf ("%s\n", text);
			}
			
			// Close a file
			else if (strcmp(cmd1, FILE_CLOSE) == 0) {
				filehandle = NULL;
			}
			
			// Quit
			else if (strcmp(cmd1, "quit") == 0) {
				printf (YELLOW "Shell exited with return status %d\n" COLOR_RESET, ret);
				break;
			}
			
			// * * * * WRONG COMMAND * * * *
			else {
				printf (YELLOW "UNKNOWN COMMAND - Type 'help .' for command list\n" COLOR_RESET);
			}
			
		} // END WHILE
		
	}


}
