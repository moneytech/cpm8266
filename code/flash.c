#include "c_types.h"
#include "eagle_soc.h"
#include "ets_sys.h"

#ifdef NOSDK
 #include "esp8266_auxrom.h"
 #include "esp8266_rom.h"
 #include "nosdk8266.h"
#endif

#ifdef WIFI
 void *ets_memcpy(void *dest, const void *src, size_t n);
 // #include "esp8266_rom.h"
 #include "espincludes.h"
 #include "spi_flash.h"
#endif

#include "z80/z80emu.h"
#include "z80/z80user.h"
#include "gpio16.h"
#include "flash.h"

#define DISKCOUNT       15    // We have 15 floppy simulated drives A..O
#define SECTORSIZE      128   // CP/M Sectors are 128 bytes each
#define SECTORSPERTRACK 26    // A standard 8" IBM disk have 26 sectors...
#define TRACKSPERDISK   77    // ... and 77 tracks
#define DISKSIZE        (SECTORSIZE*SECTORSPERTRACK*TRACKSPERDISK)
#define DISKPITCH	0x3f000 // Disks must be at sector boundries

#ifdef NOSDK
 #define DISKFLASHOFFSET 0x3c0000 // Location in flash for first disk going backwards
#endif
#ifdef WIFI
 #define DISKFLASHOFFSET 0x3bc000 // Location in flash for first disk going backwards
#endif

#define FLASHBLOCKSIZE  4096     // Nr of bytes for erase/write to flash

extern MACHINE machine;

static uint8_t __attribute__((aligned(4))) flashBuf[FLASHBLOCKSIZE];
static uint16_t flashSectorNo = 0xFFFF;
static bool dirty=false;

//
//
//
void ICACHE_FLASH_ATTR ReadDiskBlock(uint16_t mdest, uint8_t sectorNo,
                                     uint8_t trackNo, uint8_t diskNo) {
#ifdef NOSDK
  if (diskNo > (DISKCOUNT - 1)) diskNo = DISKCOUNT - 1;
#endif
#ifdef WIFI
  // Map disk N and O onto the same disk since we have less free flash
  if (diskNo > (DISKCOUNT - 2)) diskNo = DISKCOUNT - 2;
#endif
  if (trackNo > (TRACKSPERDISK - 1)) trackNo = TRACKSPERDISK - 1;
  if (sectorNo > (SECTORSPERTRACK - 1)) sectorNo = SECTORSPERTRACK - 1;

  uint32_t lba = SECTORSPERTRACK * trackNo + sectorNo;
  uint32_t flashloc = (DISKFLASHOFFSET - DISKPITCH * diskNo) + SECTORSIZE * lba;
  uint16_t myFlashSectorNo = flashloc / FLASHBLOCKSIZE;
  uint16_t fl = flashloc % FLASHBLOCKSIZE;

#ifdef WIFI
  if (myFlashSectorNo != flashSectorNo) {
    if (dirty) {
      FlushDisk(false); // Flush Disk in non-standalone mode
    }
    flashSectorNo = myFlashSectorNo;
    spi_flash_read(flashSectorNo * FLASHBLOCKSIZE, (uint32_t*)flashBuf,  FLASHBLOCKSIZE);
  }
#endif

#ifdef NOSDK 
  // Do we already have this disk sector in the bigger flash block that is already
  // read into memory (and might have dirty blocks on it that needs to be flushed first)
  if (myFlashSectorNo != flashSectorNo) {
    Cache_Read_Disable();
    SPIUnlock();
    if (dirty) {
      FlushDisk(false); // Flush Disk in non-standalone mode
    }
    flashSectorNo = myFlashSectorNo;
    SPIRead(flashSectorNo * FLASHBLOCKSIZE, (uint32_t *)flashBuf, FLASHBLOCKSIZE);
    Cache_Read_Enable(0, 0, 1);
  }
#endif

  ets_memcpy(&machine.memory[mdest], &flashBuf[fl], SECTORSIZE);
}

//
//
//
void ICACHE_FLASH_ATTR WriteDiskBlock(uint16_t msrc, uint8_t sectorNo,
                                      uint8_t trackNo, uint8_t diskNo) {
#ifdef NOSDK
  if (diskNo > (DISKCOUNT - 1)) diskNo = DISKCOUNT - 1;
#endif
#ifdef WIFI
  // Map disk N and O onto the same disk since we have less free flash
  if (diskNo > (DISKCOUNT - 2)) diskNo = DISKCOUNT - 2;
#endif
  if (trackNo > (TRACKSPERDISK - 1)) trackNo = TRACKSPERDISK - 1;
  if (sectorNo > (SECTORSPERTRACK - 1)) sectorNo = SECTORSPERTRACK - 1;

  uint32_t lba = SECTORSPERTRACK * trackNo + sectorNo;
  uint32_t flashloc = (DISKFLASHOFFSET - DISKPITCH * diskNo) + SECTORSIZE * lba;
  uint16_t myFlashSectorNo = flashloc / FLASHBLOCKSIZE;
  uint16_t fl = flashloc % FLASHBLOCKSIZE;

#ifdef WIFI
  if (myFlashSectorNo != flashSectorNo) {
    if (dirty) {
      FlushDisk(false); // Flush Disk in non-standalone mode
    }
    flashSectorNo = myFlashSectorNo;
    spi_flash_read(flashSectorNo * FLASHBLOCKSIZE, (uint32_t*)flashBuf,  FLASHBLOCKSIZE);
  }
#endif

#ifdef NOSDK
  if (myFlashSectorNo != flashSectorNo) {
   Cache_Read_Disable();
    SPIUnlock();
    if (dirty) {
      FlushDisk(false); // Flush Disk in non-standalone mode
    }
    flashSectorNo = myFlashSectorNo;
    SPIRead(flashSectorNo * FLASHBLOCKSIZE, (uint32_t *)flashBuf, FLASHBLOCKSIZE);
    Cache_Read_Enable(0, 0, 1);
  }
#endif

  ets_memcpy(&flashBuf[fl], &machine.memory[msrc],  SECTORSIZE);
  gpio16_output_set(0);
  dirty=true;
}


//
//
//
void ICACHE_FLASH_ATTR FlushDisk(bool standalone) {
  if (!dirty) return;

#ifdef WIFI
  spi_flash_erase_sector(flashSectorNo);
  spi_flash_write(flashSectorNo * FLASHBLOCKSIZE, (uint32_t *)flashBuf, FLASHBLOCKSIZE);
#endif

#ifdef NOSDK
  if (standalone) {
    Cache_Read_Disable();
    SPIUnlock();
  }
  SPIEraseSector(flashSectorNo);
  SPIWrite(flashSectorNo * FLASHBLOCKSIZE, (uint32_t *)flashBuf, FLASHBLOCKSIZE);
  if (standalone) {
   Cache_Read_Enable(0, 0, 1);
  }
#endif

  gpio16_output_set(1);
  dirty=false;
}

