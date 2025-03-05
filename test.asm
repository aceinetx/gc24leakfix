reboot: jmp main

write:
  dex %cx
.loop:
  lodb %si %ax
  push %ax
  int $02
  loop .loop
  ret

puti:
  mov %gi puti_buf
  add %gi 7
.loop:
  div %ax 10 ; Divide and get the remainder into %dx
  add %dx 48 ; Convert to ASCII
  stob %gi %dx
  sub %gi 2
  cmp %ax $00
  jne .loop

  mov %si puti_buf
  mov %cx 8
  call write
  ret
puti_buf: reserve 8 bytes

main:
  mov %ax 69
  call puti
  push '$'
  int 2
  hlt
