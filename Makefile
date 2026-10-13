CC     = aztec42_cc
AS     = aztec42_as
STRIP  = aztec42_sqz
LD     = aztec42_link
LIB    = aztec42_lib
ASM86  = cpm86_asm86
GENCMD = cpm86_gencmd
LDFLAGS = -lc86

# Flags common to both CP/M-86 builds
CFLAGS = -I. +F +0

# --------------------------------------------------------------------
# Top-level targets
# --------------------------------------------------------------------
all: menu.cmd msub.cmd hello.cmd mclean.cmd mcheck.cmd

# --------------------------------------------------------------------
# Link targets
# --------------------------------------------------------------------
menu.cmd: menu.o menudat.o util.lib 
	$(LD) -o $@ menu.o menudat.o util.lib $(LDFLAGS)

msub.cmd: msub.o util.lib
	$(LD) -o $@ msub.o util.lib $(LDFLAGS)

hello.cmd: hello.o util.lib
	$(LD) -o $@ hello.o util.lib $(LDFLAGS)

# MCHECK: the MENU parser built with -DMCHECK (reports what it skips)
# util.lib twice: subfile.o (sub_readln) needs sub_append from sub.o, which
# the one-pass linker has already passed
mcheck.cmd: mcheck.o menudat_chk.o util.lib
	$(LD) -o $@ mcheck.o menudat_chk.o util.lib util.lib $(LDFLAGS)

menudat_chk.o: menudat.c menudat.h
	$(CC) $(CFLAGS) -DMCHECK -o $@ menudat.c
	$(STRIP) $@

# MCLEAN is assembler, 8080 model (CS = DS = ES = SS)
mclean.cmd: mclean.h86
	$(GENCMD) $< 8080

mclean.h86: mclean.a86
	$(ASM86) $<

# --------------------------------------------------------------------
# Compile rules
# --------------------------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<
	$(STRIP) $@

os.o: os.asm
	$(AS) $<
	$(STRIP) $@

# --------------------------------------------------------------------
# Test image
# --------------------------------------------------------------------
cpmtest.img: menu.cmd msub.cmd hello.cmd mclean.cmd mcheck.cmd menu.dat menu1.dat menu2.dat batch.sub nest.sub soak/*
	cp cpmbase.img cpmtest.img
	cpmcp -f ibmpc-514ss cpmtest.img menu.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img msub.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img hello.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img mclean.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img mcheck.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img *.dat 0:
	cpmcp -f ibmpc-514ss cpmtest.img batch.sub 0:
	cpmcp -f ibmpc-514ss cpmtest.img nest.sub 0:
	cpmcp -f ibmpc-514ss cpmtest.img soak/* 0:
	cpmls -F -f ibmpc-514ss cpmtest.img 

# --------------------------------------------------------------------
# Binary zip
# --------------------------------------------------------------------
# Programs, the sample menus/jobs they use (HELLO is the demo program the
# samples launch), and the soak test. -j: flat archive, no soak/ folder.
DIST_CMD  = menu.cmd msub.cmd hello.cmd mclean.cmd mcheck.cmd
DIST_DAT  = menu.dat menu1.dat menu2.dat soak/soak.dat
DIST_SUB  = batch.sub nest.sub soak/soak.sub soak/soaka.sub soak/soakb.sub soak/soakm.sub

dist: msub.zip

msub.zip: $(DIST_CMD) $(DIST_DAT) $(DIST_SUB)
	rm -f msub.zip
	zip -j msub.zip $^

# --------------------------------------------------------------------
# Utility
# --------------------------------------------------------------------
clean:
	$(RM) cpmtest.img *.o *.h86 *.lst *.sym menu.cmd msub.cmd hello.cmd mclean.cmd mcheck.cmd msub.zip

test: cpm86test

cpm86test: cpmtest.img
	./cpm86
