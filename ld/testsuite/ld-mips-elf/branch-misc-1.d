#name: MIPS branch-misc-1
#source: ../../../gas/testsuite/gas/mips/branch-misc-1.s
#objdump: --prefix-addresses -tdr --show-raw-insn
#ld: -Ttext 0x400000 -e 0x400000

.*:     file format elf.*mips.*

#...

Disassembly of section \.text:
	\.\.\.
	\.\.\.
	\.\.\.
0+40003c <[^>]*> 0411fff0 	bal	0+400000 <[^>]*>
0+400040 <[^>]*> 00000000 	nop
0+400044 <[^>]*> 0411fff3 	bal	0+400014 <[^>]*>
0+400048 <[^>]*> 00000000 	nop
0+40004c <[^>]*> 0411fff6 	bal	0+400028 <[^>]*>
0+400050 <[^>]*> 00000000 	nop
0+400054 <[^>]*> 0411000a 	bal	0+400080 <[^>]*>
0+400058 <[^>]*> 00000000 	nop
0+40005c <[^>]*> 0411000d 	bal	0+400094 <[^>]*>
0+400060 <[^>]*> 00000000 	nop
0+400064 <[^>]*> 04110010 	bal	0+4000a8 <[^>]*>
0+400068 <[^>]*> 00000000 	nop
	\.\.\.
	\.\.\.
	\.\.\.
	\.\.\.
#pass
