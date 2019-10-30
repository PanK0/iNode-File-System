#pragma once
#include "../FS/inodefs.c"
#include <stdlib.h>

#define NUM_BLOCKS 	500
#define NUM_FILES	400
#define MAX_CMD_LEN	128
#define BACK		".."

#define FILE_0	"Hell0"
#define FILE_1	"POt_aTO"
#define FILE_2	"MunneZ"
#define FILE_3	"cocumber"

#define DIR_0	"LOL"
#define DIR_1	"LEL"
#define DIR_2	"LUL"
#define DIR_3	"COMPITI"

#define BOLD_RED	 		"\033[1m\033[31m"
#define BOLD_YELLOW 		"\033[1m\033[33m"
#define RED     			"\x1b[31m"
#define YELLOW  			"\x1b[33m"
#define COLOR_RESET   		"\x1b[0m"

#define SYS_SHOW	"status"
#define SYS_HELP	"help"

#define DIR_SHOW	"where"
#define DIR_CHANGE	"cd"
#define DIR_MAKE	"mkdir"
#define DIR_MAKE_N	"mkndir"
#define DIR_LS		"ls"
#define DIR_REMOVE	"rm"

#define	FILE_MAKE	"mkfil"
#define	FILE_MAKE_N	"mknfil"
#define FILE_SHOW	"show"
#define FILE_OPEN	"open"
#define FILE_WRITE	"write"
#define FILE_SEEK	"seek"
#define FILE_READ	"cat"
#define FILE_CLOSE	"fclose"

// Prints a FirstDirectoryBlock
void iNodeFS_printFirstDir(iNodeFS* fs, DirectoryHandle* d);

// Prints the Disk Driver content
void iNodeFS_print (iNodeFS* fs, DirectoryHandle* d);

// Prints the current directory location
void iNodeFS_printHandle (void* h);

// Generates names for files
void gen_filename(char *s, const int len);

// Generates names for directories
void gen_dirname(char *s, int i);
