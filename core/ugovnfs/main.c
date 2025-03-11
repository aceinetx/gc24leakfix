#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

// The header is the first 32 bytes of the disk
uint8_t readHeader(uint8_t* disk) {
  puts("Disk info:");
  printf("  Filesystem:\tGovnFS 2.0\n", disk[0x00]);
  printf("  Serial:\t%02X%02X%02X%02X\n", disk[0x0C], disk[0x0D], disk[0x0E], disk[0x0F]);
  printf("  Disk letter:\t%c/\n", disk[0x10]);
  return 0;
}

uint16_t firstFileSector(uint8_t* disk, uint8_t* filename, uint8_t* tag) {
  // Compile the filename and tag into a GovnFS 2.0 header
  uint8_t combined[16];
  strcpy(combined, filename);
  memcpy(combined+13, tag, 3);

  uint16_t sector = 0x0001;
  while (disk[sector*512] != 0xF7) {
    if ((disk[sector*512] == 0x01) && (memcmp(disk+sector*512, combined, 16))) {
      printf("File #/%s/%s found\n", filename, tag);
      return sector;
    }
    sector++;
  }
  printf("File #/%s/%s was not found\n", filename, tag);
  return 0;
}

uint16_t nextLink(uint8_t* disk, uint16_t sector) {
  return (((disk[sector*512+0xFF])<<8) + (disk[sector*512+0xFE]));
}

uint8_t getFileSize(uint8_t* disk, uint8_t* filename, uint8_t* tag) {
  uint16_t fs = firstFileSector(disk, filename, tag);
  uint32_t filesize = 494;
  while (fs = nextLink(disk, fs)) filesize += 494;
  return filesize;
}

uint8_t readFile(uint8_t* disk, uint8_t* filename, uint8_t* tag) {
}

// The header is the first 32 bytes of the disk
uint8_t readFilenames(uint8_t* disk, char c) {
  printf("Listing %c/\n", c);
  uint16_t sector = 0x0001; // Sector 0 is header data
  while (disk[sector*512] != 0xF7) {
    if (disk[sector*512] == 0x01) {
      printf("  %.11s\t%.3s\n", &(disk[sector*512+1]), &(disk[sector*512+13]));
    }
    sector++;
  }
  return 0;
}

// CLI tool to make GovnFS partitions
int main(int argc, char** argv) {
  if (argc == 1) {
    puts("ugovnfs: no arguments given");
    return 1;
  }
  if (argc == 2) {
    puts("ugovnfs: no disk/flag given");
    return 1;
  }
  FILE* fl = fopen(argv[2], "rb");
  if (fl == NULL) {
    printf("ugovnfs: \033[91mfatal error:\033[0m file `%s` not found\n", argv[1]);
    return 1;
  }
  fseek(fl, 0, SEEK_END);
  uint32_t flsize = ftell(fl);
  uint8_t* disk = malloc(flsize);
  fseek(fl, 0, SEEK_SET);
  fread(disk, 1, flsize, fl);
  fclose(fl);

  // Check the disk
  if (disk[0x000000] != 0x42) {
    printf("ugovnfs: \033[91mdisk corrupted:\033[0m unknown disk header magic byte `$%02X`\n", disk[0x000000]);
    free(disk);
    return 1;
  }

  if (!strcmp(argv[1], "-i")) {
    return readHeader(disk);
  }
  else if (!strcmp(argv[1], "-l")) {
    return readFilenames(disk, disk[0x10]);
  }
  else if (!strcmp(argv[1], "-s")) {
    uint16_t fs = firstFileSector(disk, argv[3], argv[4]);
    printf("The file #/%s/%s starts at #%06X\n", argv[3], argv[4], fs*512);
    return 0;
  }
  else {
    printf("ugovnfs: \033[91mfatal error:\033[0m unknown argument: `%s`\n", argv[1]);
    free(disk);
    return 1;
  }
  puts("ugovnfs: no arguments given");
  return 0;
}
