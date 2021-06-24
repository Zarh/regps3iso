// Note from cobra ODE team
/*
	If you wish to generate a valid ISO file programmatically, or implement your own
version of genps3iso, then we provide you with the specifications of the iso file so it can be
recognized by the PS3 as a valid disc image.
	
	A disc image is a normal iso file with the exception of the first two sectors which contain
information that will be used by the PS3. The iso file's content can either be encrypted or
plain text, the first sector defining which sector regions are encrypted and which aren't.
Another consideration is that the PS3 will always read in 64KB blocks, 32 sectors at a time
of 2048 bytes each. An encrypted region must always start and end at the 0x20 sector
index boundary.

The first sector has the following format:
32 bit big endian integer: number of plain regions in the disc image
32 bit zeroes
array of <num_plain_regions> containing :
32 bit big endian integer: start sector of the plain region
32 bit big endian integer: end sector of the plain region (inclusive)

The encrypted regions are any region that exists between plain regions. For example:
00000000 00 00 00 02 00 00 00 00 00 00 00 00 00 00 02 3f
00000010 00 00 07 c0 00 00 08 5f 00 00 00 00 00 00 00 00

Configuration excerpt 5.8: ISO encrypted and plain regions.

This iso has 2 plain regions, starting from sector 0 to 0x23F and from 0x7C0 to 0x85F, and
one encrypted region from 0x240 to 0x7DF. The total number of sectors in the iso would
be 0x860 sectors.

All SELF files must reside on encrypted sectors, as well as the LICDIR/LIC.DAT license
file.
*/

// Note from Zar
/* 
I made get_plain_files, get_enc_files and read_region_info to analyse 
how plain and encrypted region are defined most of the time.
I used the headers extracted from a large amount of IRD (+2000).

From what I saw, I built this tools to defines plain & encrypted regions like that :
	
	ENCRYPTED
		/PS3_GXXX/USRDIR 	SELF must be encrypted. Other files don't matter, I chose to always encrypt them.
		/PS3_GXXX/LICDIR 	LIC.DAT must be encrypted
		/PS3_GXXX/TROPDIR 	Doesn't matter, I chose to always encrypt it.
		
	Everything else MUST be PLAIN !

Note: It doesn't really encrypt the files, it just defines the region where the file is as 'not plain', so, encrypted.
*/

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdint.h>

#if defined (__MSVCRT__)
#undef __STRICT_ANSI__ // ugly
#include <string.h>
#define strcasecmp	_strcmpi
#define stat _stati64
#else
#include <string.h>
#endif

#include "iso.h"

#define FAILED 	0
#define SUCCESS 1
#define NO		0
#define YES		1

#define ENDIAN_SWAP_32(x)		(((x) & 0x000000FF) << 24 | ((x) & 0x0000FF00) << 8 | \
								 ((x) & 0x00FF0000) >>  8 | ((x) & 0xFF000000) >> 24  )
								 
// 64KB block size read by ps3
#define _64KB_	0x20 * 0x800

void print_help()
{
	printf("v0.1\regps3iso my_iso.iso\n");
	exit(0);
}


char *GetExtension(char *path)
{
    int n = strlen(path);
    int m = n;

    while(m > 1 && path[m] != '.' && path[m] != '/') m--;
    
	
    if(strcmp(&path[m], ".0")==0 || strcmp(&path[m], ".66600")==0) { // splitted
       m--;
       while(m > 1 && path[m] != '.' && path[m] != '/') m--; 
    }
	
	if(strcasecmp(&path[m], ".bin")==0) {
		if(strcasecmp(&path[m-7], ".header.bin")==0) {
			m-=7;	
		}
	}
  
    if(path[m] == '.') return &path[m];

    return &path[n];
}

void old_ode_fix_iso(char *path)
{
	
	struct stat s;
	
	if(stat(path, &s) != 0) {
		printf("Error : file doesn't exist %s", path);
		return;
	} 
	
	FILE *f=fopen(path, "rb+");
	if( f==NULL )  {
		printf("Error : cannot open %s", path);
		return;
	}
	
	u64 blocks = s.st_size /( _64KB_ ) ;
	
	u8 *buffer = (u8 *) malloc( _64KB_ ); 
	if( buffer == NULL) {
		printf("Error : failed to malloc");
		return;
	}
	
	u32 plain_n = 0;
	u32 first_sector[32]={0};
	u32 last_sector[32]={0};
	
	u8 encrypted = NO;
	u64 i,j, k=0;
	for( i=0; i < blocks + 1; i++) {
		memset(buffer, 0, _64KB_);
		fread(buffer, _64KB_, 1, f);
		
		if( k <= (i * 100) / blocks) {
			printf("%d%\n", k);
			k+=10;
		}
		
		u8 found = NO;
		
		/// search sector per sector MAGIC of SELF (SCE) and LIC.DAT (PS3LICDA) 
		/// to know if the block is encrypted or plain
		for(j = 0; j < 0x20; j++) {
			if( memcmp((char *) buffer + j * 0x800 , (char *) "SCE", 3) == 0 
			||  memcmp((char *) buffer + j * 0x800 , (char *) "PS3LICDA", 8) == 0 )
			{
				printf( "SELF or LIC.DAT found at block = %llX, sector %llX, offset %llX\n", i, i*0x20+j, (u64) ((i*0x20+j) * 0x800));		
				found = YES;
				break;
			}
		}
		
		if( found ) {
			if( encrypted == NO) {
				encrypted = YES;
				last_sector[plain_n] = (u32) ((u32)i*(u32)0x20+(u32)(j-1));
				printf("last %d = %X\n", plain_n, last_sector[plain_n]);
				// last_sector[plain_n-1] = i*0x20;
			}
		} else {
			if( encrypted == YES ) {
				encrypted = NO;
				plain_n++;
				u32 temp = (u32) ((u32) i * (u32) 0x20);
				first_sector[plain_n]  = temp;
				printf("first %X\n", first_sector[plain_n]);
			}
		}
	}
	last_sector[plain_n]= (u32) (s.st_size / 0x800);
	
	fclose(f);
	
	plain_n++;
	
	printf("\n");
	u32 zero  = 0;
	f=fopen("test.bin", "wb");
	fwrite(&plain_n, 4, 1, f);
	fwrite(&zero, 4, 1, f);
	int u;
	for(u=0; u<plain_n; u++) {
		//first_sector[i] =  ENDIAN_SWAP_32(first_sector[i]);
		//last_sector[i] =  ENDIAN_SWAP_32(last_sector[i]);
		printf("first %d = %X      ", u, first_sector[u]);
		printf("last %d = %X\n", u, last_sector[u]);
		
		//fwrite(&first_sector[u], 4, 1, f);
		//fwrite(&last_sector[u], 4, 1, f);
	}
	
	fclose(f);
}

void get_plain_files(char *iso)
{
	FILE *f = fopen(iso, "rb");
	if( f == NULL ) {
		printf("Error : cannot fopen %s\n", iso);
		return;
	}
	
	u32 plain_n;
	fread(&plain_n, 4, 1, f);
	
	plain_n = ENDIAN_SWAP(plain_n);
	
	u32 start[plain_n];
	u32 end[plain_n];
	u32 i, j, k;
	
	fseek(f, 8, SEEK_SET);
	for(i=0; i < plain_n; i++) {
		fread(&start[i], 4, 1, f);
		fread(&end[i], 4, 1, f);
		
		start[i] = ENDIAN_SWAP(start[i]);
		end[i] = ENDIAN_SWAP(end[i]);
	}
	fclose(f);
	
	file_info_t *file_info=NULL;
	u32 file_number;
	
	GetFileInfo(iso, &file_info, &file_number);
	
	u32 files_n = 0;
	char **files=NULL;
	u32 *amount=NULL;
	
	char line[512];
	
	// read db
	FILE *l = fopen("plain_files.txt", "r+");
	if( l != NULL ) {
	
		while(fgets(line, 512, l) != NULL) {
			if(line[0]=='\r' || line[0]=='\n') continue;
			
			if(strstr(line, "\r") != NULL) strtok(line, "\r");
			if(strstr(line, "\n") != NULL) strtok(line, "\n");
			
			files = (char **) realloc(files, (files_n+1) * sizeof(char *));
			amount = (u32 *) realloc(amount, (files_n+1) * sizeof(u32));
			
			sscanf(line, "%08X", &amount[files_n]);
			files[files_n] = strcpy_malloc(line + 11);
			
			files_n++;
			
			memset(line, 0, 512);
		}
		
		
		fclose(l);
	}
	
	for(i=0; i < plain_n; i++) {	
		for(j=0; j<file_number; j++) {
			if( start[i] <= file_info[j].sector ) {
				if(end[i] <= file_info[j].sector ) break;
				
				u8 found = NO;
				for(k=0; k<files_n; k++) {
					if( memcmp(files[k], file_info[j].path, strlen(file_info[j].path)) == 0 ) {
						found = YES;
						amount[k]++;;
						break;
					}
				}
				if( found == NO ) {
					files = (char **) realloc(files, (files_n+1) * sizeof(char *));
					amount = (u32 *) realloc(amount, (files_n+1) * sizeof(u32));
					files[files_n] = strcpy_malloc(file_info[j].path);
					amount[files_n]=1;
					
					files_n++;
				}
			}
		}
	}
	
	// write db
	char str[512];
	l = fopen("plain_files.txt", "w");
	
	for(i=0; i<files_n; i++) {
	
		u32 max_amount = 0;	
		// search_max
		for( j = 0; j<files_n; j++) {
			if(amount[max_amount] < amount[j]) max_amount=j;
		}
		sprintf(str, "%08X | %s\n", amount[max_amount], files[max_amount]);
		fputs(str, l);
		
		amount[max_amount] = 0;
	}
	fclose(l);
	
	for(j=0; j < files_n; j++) {
		FREE(files[j]);
	}
	FREE(amount);
	FREE(files);
	
	for(j=0; j < file_number; j++) FREE(file_info[j].path)
	FREE(file_info);
	
	fclose(l);
}

void get_enc_files(char *iso)
{
	FILE *f = fopen(iso, "rb");
	if( f == NULL ) {
		printf("Error : cannot fopen %s\n", iso);
		return;
	}
	
	u32 plain_n;
	fread(&plain_n, 4, 1, f);
	
	plain_n = ENDIAN_SWAP(plain_n);
	
	u32 start[plain_n];
	u32 end[plain_n];
	u32 i, j, k;
	
	fseek(f, 8, SEEK_SET);
	for(i=0; i < plain_n; i++) {
		fread(&start[i], 4, 1, f);
		fread(&end[i], 4, 1, f);
		
		start[i] = ENDIAN_SWAP(start[i]);
		end[i] = ENDIAN_SWAP(end[i]);
	}
	fclose(f);
	
	file_info_t *file_info=NULL;
	u32 file_number;
	
	GetFileInfo(iso, &file_info, &file_number);
	
	u32 files_n = 0;
	char **files=NULL;
	u32 *amount=NULL;
	
	char line[512];
	
	// read db
	FILE *l = fopen("enc_files.txt", "r+");
	if( l != NULL ) {
	
		while(fgets(line, 512, l) != NULL) {
			if(line[0]=='\r' || line[0]=='\n') continue;
			
			if(strstr(line, "\r") != NULL) strtok(line, "\r");
			if(strstr(line, "\n") != NULL) strtok(line, "\n");
			
			files = (char **) realloc(files, (files_n+1) * sizeof(char *));
			amount = (u32 *) realloc(amount, (files_n+1) * sizeof(u32));
			
			sscanf(line, "%08X", &amount[files_n]);
			files[files_n] = strcpy_malloc(line + 11);
			
			files_n++;
			
			memset(line, 0, 512);
		}
		
		
		fclose(l);
	}
	
	
	for(i=0; i < plain_n; i++) {	
		for(j=0; j<file_number; j++) {
			if( i+1 < plain_n) {
				if( end[i]+1 <= file_info[j].sector ) {
					if(start[i+1] <= file_info[j].sector ) break;
					u8 found = NO;
					u32 len = strlen(file_info[j].path);
					for(k=0; k<files_n; k++) {
						
						if( 10 < len ) {
							if( memcmp(file_info[j].path + 10, "TROPDIR", 7) == 0 ) {
								if( memcmp(file_info[j].path, files[k], 18) == 0 ){
									found = YES;
									amount[k]++;;
									break;
								}
							} else
							if( memcmp(file_info[j].path + 10, "USRDIR", 6) == 0 ) {
								if( memcmp(file_info[j].path, files[k], 17) == 0 ){
									found = YES;
									amount[k]++;;
									break;
								}
							} else 
							if( strcmp(files[k], file_info[j].path) == 0 ) {
								found = YES;
								amount[k]++;;
								break;
							}
						} else
						if( strcmp(files[k], file_info[j].path) == 0 ) {
							found = YES;
							amount[k]++;;
							break;
						}
					}
					if( found == NO ) {
						files = (char **) realloc(files, (files_n+1) * sizeof(char *));
						amount = (u32 *) realloc(amount, (files_n+1) * sizeof(u32));
						if( 10 < len ) {
							if( memcmp(file_info[j].path + 10, "TROPDIR", 7) == 0 ) {
								char temp[512]={0};
								memset(temp, 0, 512);
								strncpy(temp, file_info[j].path, 18);
								files[files_n] = strcpy_malloc(temp);
								amount[files_n]=1;
							} else
							if( memcmp(file_info[j].path + 10, "USRDIR", 6) == 0 ) {
								char temp[512]={0};
								memset(temp, 0, 512);
								strncpy(temp, file_info[j].path, 17);
								files[files_n] = strcpy_malloc(temp);
								amount[files_n]=1;
							} else {
								files[files_n] = strcpy_malloc(file_info[j].path);
								amount[files_n]=1;
							}
						} else {
							files[files_n] = strcpy_malloc(file_info[j].path);
							amount[files_n]=1;
						}
						files_n++;
					}
				}
			}
		}
	}
	
	// write db
	char str[512];
	l = fopen("enc_files.txt", "w");
	
	for(i=0; i<files_n; i++) {
	
		u32 max_amount = 0;	
		// search_max
		for( j = 0; j<files_n; j++) {
			if(amount[max_amount] < amount[j]) max_amount=j;
		}
		sprintf(str, "%08X | %s\n", amount[max_amount], files[max_amount]);
		fputs(str, l);
		
		amount[max_amount] = 0;
	}
	fclose(l);
	
	for(j=0; j < files_n; j++) {
		FREE(files[j]);
	}
	FREE(amount);
	FREE(files);
	
	for(j=0; j < file_number; j++) FREE(file_info[j].path)
	FREE(file_info);
	
	fclose(l);
}

void read_region_info(char *iso)
{
	FILE *f = fopen(iso, "rb");
	if( f == NULL ) {
		printf("Error : cannot fopen %s", iso);
		return;
	}
	
	char log[512];
	char str[1024]={0};
	sprintf(log, "%s.region.txt", iso);
	
	FILE *l = fopen(log, "wb");
	if( l == NULL ) {
		printf("Error : cannot fopen %s", log);
		fclose(f);
		return;
	}
	
	u32 plain_n;
	fread(&plain_n, 4, 1, f);
	
	plain_n = ENDIAN_SWAP(plain_n);
	
	u32 start[plain_n];
	u32 end[plain_n];
	u32 i, j;
	
	sprintf(str, "Plain number = %d\n", plain_n); fputs(str, l);
	fseek(f, 8, SEEK_SET);
	for(i=0; i < plain_n; i++) {
		fread(&start[i], 4, 1, f);
		fread(&end[i], 4, 1, f);
		
		start[i] = ENDIAN_SWAP(start[i]);
		end[i] = ENDIAN_SWAP(end[i]);
		
		if( i != 0 ) {
			sprintf(str, "0x%08X\n", start[i] - 1); 
			fputs(str, l);
		}
		
		sprintf(str, "Plain 0x%08X - 0x%08X\n", start[i], end[i]); fputs(str, l);
		if( i+1 < plain_n ) {
			sprintf(str, "**Enc 0x%08X - ", end[i] + 1); fputs(str, l);
		}
	}
	fclose(f);
	
	file_info_t *file_info=NULL;
	u32 file_number;
	
	GetFileInfo(iso, &file_info, &file_number);
	
	fputs("\n\n\n", l);
	
	fflush(l);
		
	for(i=0; i < plain_n; i++) {
			sprintf(str, "--- PLAIN               ---  %08X\n",  start[i]); fputs(str, l); fflush(l);
		if(i==0) fputs("\tsector 00000000 | Header\n", l); fflush(l);
				
		for(j=0; j<file_number; j++) {
			if( start[i] <= file_info[j].sector ) {
				if(end[i] <= file_info[j].sector ) break;
									
				sprintf(str, "\tsector %08X | %s\n", file_info[j].sector, file_info[j].path); fputs(str, l); fflush(l);
			}
		}
		if(i+1 == plain_n) {
			u64 footer = file_info[file_number-1].sector*0x800ULL  + file_info[file_number-1].size;
			footer = (footer + 2047)/2048;
			sprintf(str, "\tsector %08X | footer\n", footer); fputs(str, l); fflush(l);		
		}
			sprintf(str, "--- End of PLAIN        ---  %08X\n\n",  end[i]); fputs(str, l); fflush(l);
		if( i+1 < plain_n) {
			sprintf(str, "--- ENCRYPTED           ---  %08X\n",  end[i]+1); fputs(str, l); fflush(l);
			for(j=0; j<file_number; j++) {
				if( end[i]+1 <= file_info[j].sector ) {
					if(start[i+1] <= file_info[j].sector ) break;
										
					sprintf(str, "\tsector %08X | %s\n", file_info[j].sector, file_info[j].path); fputs(str, l); fflush(l);
				}
			}
			sprintf(str, "--- End of ENCRYPTED    ---  %08X\n\n",  start[i+1]-1); fputs(str, l); fflush(l);
		}
	}
	
	
	for(j=0; j < file_number; j++) FREE(file_info[j].path)
	FREE(file_info);
	
	fclose(l);
}

void fixps3iso_region(char *iso)
{
	struct stat s;
	FILE *f = NULL;
	
	if(stat(iso, &s) != 0) {
		printf("Error : file doesn't exist %s", iso);
		return;
	} 
	
	file_info_t *file_info=NULL;
	u32 file_number;
	
	GetFileInfo(iso, &file_info, &file_number);
	
	u32 i;
	u32 plain_n=1;
	u32 *start = (u32 *) malloc(plain_n * sizeof(u32) );
	u32 *end = malloc(plain_n * sizeof(u32) );
	start[plain_n-1] = 0;
	
	u8 is_plain=YES;
	for(i=0; i<file_number; i++) {	
		
		u32 len = strlen(file_info[i].path);
		if( (memcmp(file_info[i].path, "/PS3_GAME/", 10) == 0 || memcmp(file_info[i].path, "/PS3_GM", 7) == 0) && 10 < len ) {
			
			if( memcmp(file_info[i].path + 10, "TROPDIR", 7) == 0 
			||  memcmp(file_info[i].path + 10, "USRDIR" , 6) == 0 
			||  memcmp(file_info[i].path + 10, "LICDIR" , 6) == 0 ) {
				if( is_plain == YES ) {
					is_plain = NO;
					end[plain_n - 1] = file_info[i].sector - 1;
				}
			} else {
				if( is_plain == NO ) {
					is_plain = YES;
					plain_n++;
					start = realloc(start, plain_n * sizeof(u32));
					end = realloc(end, plain_n * sizeof(u32));
					start[plain_n-1] = file_info[i].sector;
				}
			}
		} else {
			if( is_plain == NO ) {
				is_plain = YES;
				plain_n++;
				start = realloc(start, plain_n * sizeof(u32));
				end = realloc(end, plain_n * sizeof(u32));
				start[plain_n-1] = file_info[i].sector;
			}
		}
	}
	if( is_plain == NO) { // PUP is supposed to be the last file, but just in case, there is no pup or if the iso isn't 'proper'
		is_plain = YES;
		plain_n++;
		start = realloc(start, plain_n * sizeof(u32));
		end = realloc(end, plain_n * sizeof(u32));
		u64 footer = file_info[file_number-1].sector*0x800ULL  + file_info[file_number-1].size;
		footer = (footer + 2047)/2048;	
		start[plain_n-1] = (u32) footer; 
	}
	char *ext = GetExtension(iso);
	if( strcasecmp(ext, ".header.bin") == 0 ) {
		u32 current_plain_n;
		f = fopen(iso, "rb+");
		if(f==NULL ) {
			printf("Error : cannot fopen %s", iso);
			return;
		}
		fread(&current_plain_n, 4, 1, f);
		current_plain_n = ENDIAN_SWAP(current_plain_n);
		fseek(f, 8 + current_plain_n*8 - 4, SEEK_SET);
		u32 tot_sec=0;
		fread(&tot_sec, 4, 1, f);
		end[plain_n-1] = ENDIAN_SWAP(tot_sec);
		fseek(f, 0, SEEK_SET);
	} else 
	if( strcasecmp(ext, ".iso") == 0) {
		end[plain_n-1] = (u32) (s.st_size / 0x800ULL);
	}
	
	if(f==NULL) f = fopen(iso, "rb+");
	if(f==NULL ) {
		printf("Error : cannot fopen %s", iso);
		return;
	}
	plain_n = ENDIAN_SWAP(plain_n);
	fwrite(&plain_n, 4, 1, f);
	plain_n = ENDIAN_SWAP(plain_n);
	fseek(f, 8, SEEK_SET);
	for(i=0; i<plain_n; i++) {
		start[i] = ENDIAN_SWAP(start[i]);
		end[i] = ENDIAN_SWAP(end[i]);
		
		fwrite(&start[i], 4, 1, f);
		fwrite(&end[i], 4, 1, f);
	}
	fclose(f);
}

u8 is_dir(char *path)
{
	struct stat path_stat;
    stat(path, &path_stat);
    return S_ISDIR(path_stat.st_mode);
}

#define do_read_region_info 	0
#define do_get_plain_files		1
#define do_get_enc_files		2
#define do_fixps3iso_region		3

void do_it(char *path, u8 task)
{
	switch(task)
	{
		case do_read_region_info:
		{
			read_region_info(path);
		}
		break;
		case do_get_plain_files:
		{
			get_plain_files(path);
		}
		break;
		case do_get_enc_files:
		{
			get_enc_files(path);
		}
		break;
		case do_fixps3iso_region:
		{
			fixps3iso_region(path);
		}
		break;
		default:
		{
			print_help();
		}
		break;
	}
}

void do_task(char *path_in, u8 task)
{
	DIR *d;
	struct dirent *dir;
	
	char path[512];
	strcpy(path, path_in);
	int l = strlen(path);
	int i;
	
	for(i=0;i<l;i++){
		if(path[i]=='\\') path[i]='/';
	}
	
	if( is_dir(path) ) {
		d = opendir(path);
		if(d==NULL) return;
		
		while ((dir = readdir(d))) {
			if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) continue;		
			
			char temp[512];
			sprintf(temp, "%s/%s", path, dir->d_name);
			
			if(is_dir(temp)) do_task(temp, task);
			else {
				
				char *ext = GetExtension(temp);
				if( strcasecmp(ext, ".header.bin") == 0 || strcasecmp(ext, ".iso") == 0) {
					do_it(temp, task);
				}
			}
		}
		closedir(d);
	} else {
		do_it(path, task);
	}
}

u8 verbose=0;
int main (int argc, char **argv)
{	
	if(argc==1) print_help();
	
	u8 task = do_fixps3iso_region;
	verbose=0;
	
	int args = 1;
	if(strcmp(argv[args], "verbose") == 0){
		verbose=1;
		args++;
	}
	
	if(strcmp(argv[args], "do_read_region_info") == 0){
		task=do_read_region_info;
		args++;
	} else
	if(strcmp(argv[args], "do_get_plain_files") == 0){
		task=do_get_plain_files;
		args++;
	} else
	if(strcmp(argv[args], "do_get_enc_files") == 0){
		task=do_get_enc_files;
		args++;
	} else
	if(strcmp(argv[args], "do_fixps3iso_region") == 0){
		task=do_fixps3iso_region;
		args++;
	}
	
	u32 i;
	for(i=args;i<argc;i++) {
		do_task(argv[i], task);
	}
	
	return 0;
}