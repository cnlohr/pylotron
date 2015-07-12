all : fonter fps

OBJS:=cnrfb.c linux_interface.c heap.c

MAKE_TINY_x86_64:=-Os -s -flto -Wl,--relax -Wl,-Map=test.map -Wl,--gc-sections -ffunction-sections -fdata-sections -T./elf_x86_64_mod.x

fps : fps.c $(OBJS) font.h mathtables.h 
	gcc $(OBJS) $< mathfuncts.c -o $@  $(MAKE_TINY_x86_64)

fonter : fonter.o
	gcc $(MAKE_TINY_x86_64) -o $@ $^

mathtables.h : tabler.c
	gcc $(MAKE_TINY_x86_64) -o tabler tabler.c -lm
	./tabler > mathtables.h

font.h : fonter vga8x12.pbm
	./fonter vga8x12.pbm 8 16 > font.h

clean :
	rm -rf fonter cnrfb *.o *~ fps tabler

wipe : clean
	rm -rf mathtables.h font.h


