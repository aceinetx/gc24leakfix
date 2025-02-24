// Header include file for lib/cpu24/cpu24.h
#ifndef CPU16H_H
#define CPU16H_H 1

/*
  The memory and ROM size is set to their maximum values:
  16,777,216 bytes (16 MiB).
*/
#define ROMSIZE 16777216
#define MEMSIZE 16777216

union gcreg {
  uint32_t dword;
  uint16_t word;
  uint8_t hl;
};
typedef union gcreg gcreg;

// Register cluster
struct gcrc {
  gcbyte x;
  gcbyte y;
};
typedef struct gcrc gcrc_t;

struct GC24 {
  // Govno Core 24's 8 addressable registers: AX, BX, CX, DX, SI, GI, SP, BP
  gcreg reg[8];

  gcbyte PS;   // -I---ZNC                 Unaddressable
  uint32_t PC: 24; // Program counter (24-bit)          Unaddressable

  // Memory and ROM
  gcbyte mem[MEMSIZE];
  gcbyte rom[ROMSIZE];
  gcbyte pin;

  // GPU
  gc_gg16 gg;
  SDL_Renderer* renderer;
};
typedef struct GC24 GC;

#endif
