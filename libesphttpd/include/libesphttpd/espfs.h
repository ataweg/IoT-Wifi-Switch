#ifndef ESPFS_H
#define ESPFS_H

// This define is done in Makefile. If you do not use default Makefile, uncomment
// to be able to use Heatshrink-compressed espfs images.
// #define ESPFS_HEATSHRINK

typedef enum
{
   ESPFS_INIT_RESULT_OK,
   ESPFS_INIT_RESULT_NO_IMAGE,
   ESPFS_INIT_RESULT_BAD_ALIGN,
} EspFsInitResult;

typedef struct EspFsFile EspFsFile;

// here we cannot use ICACHE_FLASH_ATTR, it's in conflict with the mkespfsimage build

EspFsInitResult espFsInit( void *flashAddress );
EspFsFile *espFsOpen( const char *fileName );
int espFsFlags( EspFsFile *fh );
int espFsRead( EspFsFile *fh, char *buf, int len );
void espFsClose( EspFsFile *fh );


#endif
