// CPU identificator: GC24
#include <cpu24/proc/std.h>
#include <cpu24/proc/interrupts.h>
#include <cpu24/gpu.h>

/*
  CPU info:
  Speed: 5THz
  State: Holy 2.0
*/

#define AX 0x00
#define BX 0x01
#define CX 0x02
#define DX 0x03
#define SI 0x04
#define GI 0x05
#define SP 0x06
#define BP 0x07

#define IF(ps) (ps & 0b01000000)
#define ZF(ps) (ps & 0b00000100)
#define NF(ps) (ps & 0b00000010)
#define CF(ps) (ps & 0b00000001)

#define SET_IF(ps) (ps |= 0b01000000)
#define SET_ZF(ps) (ps |= 0b00000100)
#define SET_NF(ps) (ps |= 0b00000010)
#define SET_CF(ps) (ps |= 0b00000001)

#define RESET_IF(ps) (ps &= 0b10111111)
#define RESET_ZF(ps) (ps &= 0b11111011)
#define RESET_NF(ps) (ps &= 0b11111101)
#define RESET_CF(ps) (ps &= 0b11111110)

#define printh(c, s) printf("%02X" s, c)

U8 gc_errno;

U0 Reset(GC* gc);
U0 PageDump(GC* gc, U8 page);
U0 StackDump(GC* gc, U16 c);
U0 RegDump(GC* gc);

gcword ReadWord(GC* gc, U32 addr) {
  return (gc->mem[addr]) + (gc->mem[addr+1] << 8);
}

U32 Read24(GC* gc, U32 addr) {
  return (gc->mem[addr]) + (gc->mem[addr+1] << 8) + (gc->mem[addr+2] << 16);
}

U0 WriteWord(GC* gc, U32 addr, U16 val) {
  // Least significant byte goes first
  // $1448 -> $48,$14
  gc->mem[addr] = (val % 256);
  gc->mem[addr+1] = (val >> 8);
}

gcbyte StackPush(GC* gc, U32 val) {
  gc->mem[gc->reg[SP].word] = (val >> 16);
  gc->mem[gc->reg[SP].word-1] = ((val >> 8) % 256);
  gc->mem[gc->reg[SP].word-2] = (val % 256);
  gc->reg[SP].word -= 3;
  return 0;
}

gcword StackPop(GC* gc) {
  gc->reg[SP].word += 3;
  return Read24(gc, gc->reg[SP].word-2);
}

gcrc_t ReadRegClust(U8 clust) { // Read a register cluster
  // The register address is 4 bytes
  gcrc_t rc = {((clust&0b00111000)>>3), (clust&0b00000111)};
  return rc;
}

U8 UNK(GC* gc) {    // Unknown instruction
  fprintf(stderr, "\033[31mIllegal\033[0m instruction \033[33m%02X\033[0m\nAt position %06X\n", gc->mem[gc->PC], gc->PC);
  old_st_legacy;
  gc_errno = 1;
  return 1;
}

/* Instructions implementation start */
// 00           hlt
U8 HLT(GC* gc) {
  return 1;
}

// 01           trap
U8 TRAP(GC* gc) {
  old_st_legacy;
  ExecD(gc, 1);
  gc->PC++;
  return 0;
}

// 20-27        inx reg
U8 INXr(GC* gc) {
  gc->reg[gc->mem[gc->PC]-0x20].word++;
  gc->PC++;
  return 0;
}

// 28-2F        inx reg
U8 DEXr(GC* gc) {
  gc->reg[gc->mem[gc->PC]-0x28].word--;
  gc->PC++;
  return 0;
}

// 30           inx #imm24
U8 INXb(GC* gc) {
  gc->mem[gc, gc->PC+1]++;
  gc->PC += 4;
  return 0;
}

// 32           dex #imm24
U8 DEXb(GC* gc) {
  gc->mem[gc, gc->PC+1]--;
  gc->PC += 4;
  return 0;
}

// 40           inx @imm24
U8 INXw(GC* gc) {
  U16 addr = Read24(gc, gc->PC+1);
  U16 a = ReadWord(gc, addr)+1;
  WriteWord(gc, addr, a);
  gc->PC += 4;
  return 0;
}

// 42           dex @imm24
U8 DEXw(GC* gc) {
  U16 addr = Read24(gc, gc->PC+1);
  U16 a = ReadWord(gc, addr)-1;
  WriteWord(gc, addr, a);
  gc->PC += 4;
  return 0;
}

// 41           int imm8
U8 INT(GC* gc) {
  if (!IF(gc->PS)) goto intend;
  if (gc->mem[gc->PC+1] >= 0x80) { // Custom interrupt
    gc->PC = Read24(gc, ((gc->mem[gc->PC+1]-0x80+1)*3-3)+0xFF0000);
    return 0;
  }
  switch (gc->mem[gc->PC+1]) {
  case INT_EXIT:
    gc_errno = StackPop(gc);
    return 1;
  case INT_READ:
    StackPush(gc, getchar());
    break;
  case INT_WRITE:
    putchar(StackPop(gc));
    fflush(stdout);
    break;
  case INT_RESET:
    Reset(gc);
    return 0;
  case INT_VIDEO_FLUSH:
    GGpage(gc, gc->renderer);
    break;
  case INT_RAND:
    gc->reg[DX].word = rand() % 65536;
    break;
  case INT_DATE:
    gc->reg[DX].word = GC_GOVNODATE();
    break;
  case INT_WAIT:
    usleep((U32)(gc->reg[DX].word)*1000); // the maximum is about 65.5 seconds
    break;
  default:
    fprintf(stderr, "gc24: \033[91mIllegal\033[0m hardware interrupt: $%02X\n", gc->mem[gc->PC+1]);
    return 1;
  }
  intend: gc->PC += 2;
  return 0;
}

// 47           add rc
U8 ADDrc(GC* gc) {
  gcrc_t rc = ReadRegClust(gc->mem[gc->PC+1]);
  gc->reg[rc.x].word += gc->reg[rc.y].word;
  gc->PC += 2;
  return 0;
}

// 48-4F        add reg imm16
U8 ADDri(GC* gc) {
  gc->reg[(gc->mem[gc->PC]-0x48) % 8].word += Read24(gc, gc->PC+1);
  gc->PC += 4;
  return 0;
}

// 50-57        add reg byte[imm24]
U8 ADDrb(GC* gc) {
  gc->reg[(gc->mem[gc->PC]-0x50) % 8].word += gc->mem[Read24(gc, gc->PC+1)];
  gc->PC += 4;
  return 0;
}

// 58-5F        mov reg word[imm24]
U8 ADDrw(GC* gc) {
  gc->reg[(gc->mem[gc->PC]-0xD8) % 8].word += ReadWord(gc, Read24(gc, gc->PC+1));
  gc->PC += 4;
  return 0;
}

// 60-67        mov byte[imm24] reg
U8 ADDbr(GC* gc) {
  gc->mem[Read24(gc, gc->PC+1)] += gc->reg[(gc->mem[gc->PC]-0xE0) % 8].byte;
  gc->PC += 4;
  return 0;
}

// 68-6F        add word[imm24] reg
U8 ADDwr(GC* gc) {
  U16 addr = Read24(gc, gc->PC+1);
  U16 w = ReadWord(gc, addr);
  WriteWord(gc, addr, w+gc->reg[(gc->mem[gc->PC]-0xE8) % 8].word);
  gc->PC += 4;
  return 0;
}

// 70-77        cmp reg imm16
U8 CMPri(GC* gc) {
  I16 val0 = gc->reg[(gc->mem[gc->PC]-0x70) % 8].word;
  I16 val1 = Read24(gc, gc->PC+1);

  if (!(val0 - val1))    SET_ZF(gc->PS);
  else                   RESET_ZF(gc->PS);
  if ((val0 - val1) < 0) SET_NF(gc->PS);
  else                   RESET_NF(gc->PS);

  gc->PC += 4;
  return 0;
}

// 7F           lodb rc
U8 LODBc(GC* gc) {
  gcrc_t rc = ReadRegClust(gc->mem[gc->PC+1]);
  gc->reg[rc.y].word = gc->mem[gc->reg[rc.x].word];
  gc->reg[rc.x].word++;
  gc->PC += 2;
  return 0;
}

// 8F           lodw rc
U8 LODWc(GC* gc) {
  gcrc_t rc = ReadRegClust(gc->mem[gc->PC+1]);
  gc->reg[rc.y].word = ReadWord(gc, gc->reg[rc.x].word);
  gc->reg[rc.x].word += 2;
  gc->PC += 2;
  return 0;
}

// 9F           lodh rc
U8 LODHc(GC* gc) {
  gcrc_t rc = ReadRegClust(gc->mem[gc->PC+1]);
  gc->reg[rc.y].word = Read24(gc, gc->reg[rc.x].word);
  gc->reg[rc.x].word += 3;
  gc->PC += 2;
  return 0;
}

// 86           jmp imm24
U8 JMPa(GC* gc) {
  gc->PC = Read24(gc, gc->PC+1);
  return 0;
}

// A0           je imm24
U8 JEa(GC* gc) {
  if (ZF(gc->PS)) {
    gc->PC = Read24(gc, gc->PC+1);
    RESET_ZF(gc->PS);
  }
  else gc->PC += 4;
  return 0;
}

// A1           jne imm24
U8 JNEa(GC* gc) {
  if (!ZF(gc->PS)) {
    gc->PC = Read24(gc, gc->PC+1);
  }
  else gc->PC += 4;
  return 0;
}

// A2           jc imm24
U8 JCa(GC* gc) {
  if (CF(gc->PS)) {
    gc->PC = Read24(gc, gc->PC+1);
    RESET_CF(gc->PS);
  }
  else gc->PC += 4;
  return 0;
}

// A3           jnc imm24
U8 JNCa(GC* gc) {
  if (!CF(gc->PS)) {
    gc->PC = Read24(gc, gc->PC+1);
  }
  else gc->PC += 4;
  return 0;
}

// A4           js imm24
U8 JSa(GC* gc) {
  if (!NF(gc->PS)) {
    gc->PC = Read24(gc, gc->PC+1);
  }
  else gc->PC += 4;
  return 0;
}

// A5           jn imm24
U8 JNa(GC* gc) {
  if (NF(gc->PS)) {
    gc->PC = Read24(gc, gc->PC+1);
    RESET_NF(gc->PS);
  }
  else gc->PC += 4;
  return 0;
}

// A6           ji imm24
U8 JIa(GC* gc) {
  if (IF(gc->PS)) {
    gc->PC = Read24(gc, gc->PC+1);
    RESET_IF(gc->PS);
  }
  else gc->PC += 4;
  return 0;
}

// A7           jni imm24
U8 JNIa(GC* gc) {
  if (!IF(gc->PS)) {
    gc->PC = Read24(gc, gc->PC+1);
  }
  else gc->PC += 4;
  return 0;
}

// B0           push imm16
U8 PUSHi(GC* gc) {
  StackPush(gc, Read24(gc, gc->PC+1));
  gc->PC += 4;
  return 0;
}

// B5           push reg
U8 PUSHr(GC* gc) {
  StackPush(gc, gc->reg[gc->mem[gc->PC+1]].word);
  gc->PC += 2;
  return 0;
}

// C0-C7        mov reg imm16
U8 MOVri(GC* gc) {
  gc->reg[(gc->mem[gc->PC]-0xC0) % 8].word = Read24(gc, gc->PC+1);
  gc->PC += 4;
  return 0;
}

// CF           mov rc
U8 MOVrc(GC* gc) {
  gcrc_t rc = ReadRegClust(gc->mem[gc->PC+1]);
  gc->reg[rc.x].word = gc->reg[rc.y].word;
  gc->PC += 2;
  return 0;
}

// D0-D7        mov reg byte[imm24]
U8 MOVrb(GC* gc) {
  gc->reg[(gc->mem[gc->PC]-0xD0) % 8].word = gc->mem[Read24(gc, gc->PC+1)];
  gc->PC += 4;
  return 0;
}

// D8-E0        mov reg word[imm24]
U8 MOVrw(GC* gc) {
  gc->reg[(gc->mem[gc->PC]-0xD8) % 8].word = ReadWord(gc, Read24(gc, gc->PC+1));
  gc->PC += 4;
  return 0;
}

// E0-E7        mov byte[imm24] reg
U8 MOVbr(GC* gc) {
  gc->mem[Read24(gc, gc->PC+1)] = gc->reg[(gc->mem[gc->PC]-0xE0) % 8].byte;
  gc->PC += 4;
  return 0;
}

// E8-EF        mov word[imm24] reg
U8 MOVwr(GC* gc) {
  WriteWord(gc, Read24(gc, gc->PC+1), gc->reg[(gc->mem[gc->PC]-0xE8) % 8].word);
  gc->PC += 4;
  return 0;
}

/* Instructions implementation end */

U8 PG0F(GC*); // Page 0F - Additional instructions page

// Zero page instructions
U8 (*INSTS[256])() = {
  &HLT  , &TRAP , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &INXr , &INXr , &INXr , &INXr , &INXr , &INXr , &INXr , &INXr , &DEXr , &DEXr , &DEXr , &DEXr , &DEXr , &DEXr , &DEXr , &DEXr ,
  &INXb , &UNK  , &DEXb , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &INXw , &INT  , &DEXw , &UNK  , &UNK  , &UNK  , &UNK  , &ADDrc, &ADDri, &ADDri, &ADDri, &ADDri, &ADDri, &ADDri, &ADDri, &ADDri,
  &ADDrb, &ADDrb, &ADDrb, &ADDrb, &ADDrb, &ADDrb, &ADDrb, &ADDrb, &ADDrw, &ADDrw, &ADDrw, &ADDrw, &ADDrw, &ADDrw, &ADDrw, &ADDrw,
  &ADDbr, &ADDbr, &ADDbr, &ADDbr, &ADDbr, &ADDbr, &ADDbr, &ADDbr, &ADDwr, &ADDwr, &ADDwr, &ADDwr, &ADDwr, &ADDwr, &ADDwr, &ADDwr,
  &CMPri, &CMPri, &CMPri, &CMPri, &CMPri, &CMPri, &CMPri, &CMPri, &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &LODBc,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &JMPa , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &LODWc,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &LODHc,
  &JEa  , &JNEa , &JCa  , &JNCa , &JSa  , &JNa  , &JIa  , &JNIa , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &PUSHi, &UNK  , &UNK  , &UNK  , &UNK  , &PUSHr, &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &MOVri, &MOVri, &MOVri, &MOVri, &MOVri, &MOVri, &MOVri, &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &MOVrc,
  &MOVrb, &MOVrb, &MOVrb, &MOVrb, &MOVrb, &MOVrb, &MOVrb, &MOVrw, &MOVrw, &MOVrw, &MOVrw, &MOVrw, &MOVrw, &MOVrw, &MOVrw, &MOVrw,
  &MOVbr, &MOVbr, &MOVbr, &MOVbr, &MOVbr, &MOVbr, &MOVbr, &MOVwr, &MOVwr, &MOVwr, &MOVwr, &MOVwr, &MOVwr, &MOVwr, &MOVwr, &MOVwr,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK
};

U8 (*INSTS_PG0F[256])() = {
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  ,
  &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK  , &UNK
};

U8 PG0F(GC* gc) {   // 0FH
  gc->PC++;
  return (INSTS_PG0F[gc->mem[gc->PC]])(gc);
}

U0 Reset(GC* gc) {
  gc->reg[SP].word = 0xFEFFFF;
  gc->reg[BP].word = 0xFEFFFF;
  gc->PC = 0x030000;

  // Reset the general purpose registers
  for (U8 i = 0; i < 6; i++) 
    gc->reg[i].word = 0x000000;

  gc->PS = 0b01000000;
}

U0 PageDump(GC* gc, U8 page) {
  for (U16 i = (page*256); i < (page*256)+256; i++) {
    if (!(i % 16)) putchar(10);
    printf("%02X ", gc->mem[i]);
  }
}

U0 StackDump(GC* gc, U16 c) {
  for (U32 i = 0xFFFFFF; i > 0xFFFF-c; i--) {
    printf("%06X: %02X\n", i, gc->mem[i]);
  }
}

U0 MemDump(GC* gc, U32 start, U32 end, U8 newline) {
  for (U32 i = start; i < end; i++) {
    printf("%02X ", gc->mem[i]);
  }
  putchar(8);
  putchar(10*newline);
}

U0 RegDump(GC* gc) {
  printf("pc: %06X;  a: %06X\n", gc->PC, gc->reg[AX]);
  printf("b: %06X;     c: %06X\n", gc->reg[BX], gc->reg[CX]);
  printf("d: %06X;     s: %06X\n", gc->reg[DX], gc->reg[SI]);
  printf("g: %06X;     sp: %06X\n", gc->reg[GI], gc->reg[SP]);
  printf("ps: %08b; bp: %06X\n", gc->PS, gc->reg[BP]);
  printf("   -I---ZNC\033[0m\n");
}

U8 Exec(GC* gc, const U32 memsize) {
  U8 exc = 0;
  execloop:
    // getchar();
    exc = (INSTS[gc->mem[gc->PC]])(gc);
    // RegDump(gc);
    MemDump(gc, 0x000000, 0x000010, 1);
    if (exc != 0) return gc_errno;
    goto execloop;
  return exc;
}
