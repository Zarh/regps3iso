#ifndef _ISO_H
#define _ISO_H

#define ENDIAN_SWAP_16(x)		(((x) & 0x00FF) << 8 | ((x) & 0xFF00) >> 8)
#define ENDIAN_SWAP_32(x)		(((x) & 0x000000FF) << 24 | ((x) & 0x0000FF00) << 8 | \
								 ((x) & 0x00FF0000) >>  8 | ((x) & 0xFF000000) >> 24  )
#define ENDIAN_SWAP_64(x)		(((x) & 0x00000000000000FFULL) << 56 | ((x) & 0x000000000000FF00ULL) << 40 | \
								 ((x) & 0x0000000000FF0000ULL) << 24 | ((x) & 0x00000000FF000000ULL) <<  8 | \
								 ((x) & 0x000000FF00000000ULL) >>  8 | ((x) & 0x0000FF0000000000ULL) >> 24 | \
								 ((x) & 0x00FF000000000000ULL) >> 40 | ((x) & 0xFF00000000000000ULL) >> 56 )
#define ENDIAN_SWAP(x)			(sizeof(x) == 2 ? ENDIAN_SWAP_16(x) : (sizeof(x) == 4 ? ENDIAN_SWAP_32(x) : ENDIAN_SWAP_64(x)))
//#define ENDIAN_SWAP(x)          x

#define FREE(x)					if(x!=NULL) {free(x);x=NULL;}

typedef uint8_t		u8;
typedef uint16_t 	u16;
typedef uint32_t	u32;
typedef uint64_t 	u64;

typedef struct
{
	char *path;
	//char magic[4];
	u32 sector;
	u64 size;
} file_info_t;

u8 GetFileInfo(char *iso, file_info_t **file_info, u32 *file_number);
char *strcpy_malloc(char *STR_DEFAULT);

#endif

