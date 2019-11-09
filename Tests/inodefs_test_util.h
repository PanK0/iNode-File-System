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
#define FILE_DANTE	"dante"
#define FILE_OMERO	"omero"

char dante[] = "Nel mezzo del cammin di nostra vita mi ritrovai per una selva oscura ché la diritta via era smarrita.Ahi quanto a dir qual era è cosa dura esta selva selvaggia e aspra e forte che nel pensier rinova la paura! Tant'è amara che poco è più morte; ma per trattar del ben ch'i' vi trovai, dirò de l'altre cose ch'i' v'ho scorte. Io non so ben ridir com'i' v'intrai, tant'era pien di sonno a quel punto che la verace via abbandonai. Ma poi ch'i' fui al piè d'un colle giunto, là dove terminava quella valle che m'avea di paura il cor compunto, guardai in alto, e vidi le sue spalle vestite già de' raggi del pianeta che mena dritto altrui per ogne calle. Allor fu la paura un poco queta che nel lago del cor m'era durata la notte ch'i' passai con tanta pieta. E come quei che con lena affannata uscito fuor del pelago a la riva si volge a l'acqua perigliosa e guata, così l'animo mio, ch'ancor fuggiva, si volse a retro a rimirar lo passo che non lasciò già mai persona viva.";

char omero[] = "Cantami, o Diva, l'ira funesta del pelide Achille che infiniti lutti addusse agli achei e gettò nell'Ade innumerevoli anime di eroi e abbandonò i loro corpi a cani e uccelli. Così si compì la volontà di Zeus, da quando al tempo indusse in contesa l'atride, re di popoli, e il divino Achille.";

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

// Prints an array of strings
void iNodeFS_printArray (char** a, int len);
