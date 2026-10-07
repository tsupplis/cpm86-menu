CC     = aztec42_cc
AS     = aztec42_as
STRIP  = aztec42_sqz
LD     = aztec42_link
LIB    = aztec42_lib
LDFLAGS = -lc86

# Flags common to both CP/M-86 builds
CFLAGS = -I. +F +0

# --------------------------------------------------------------------
# Top-level targets
# --------------------------------------------------------------------
all: menu.cmd msub.cmd hello.cmd

# --------------------------------------------------------------------
# Libraries
# --------------------------------------------------------------------
util.lib: conio.o os.o
	rm -f util.lib
	$(LIB) util.lib conio.o os.o

sub.lib: sub.o subfile.o
	rm -f sub.lib
	$(LIB) sub.lib sub.o subfile.o

# --------------------------------------------------------------------
# Link targets
# --------------------------------------------------------------------
menu.cmd: menu.o menudat.o util.lib sub.lib
	$(LD) -o $@ menu.o menudat.o util.lib sub.lib $(LDFLAGS)

msub.cmd: msub.o util.lib sub.lib
	$(LD) -o $@ msub.o util.lib sub.lib $(LDFLAGS)

hello.cmd: hello.o util.lib
	$(LD) -o $@ hello.o util.lib $(LDFLAGS)

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
cpmtest.img: menu.cmd msub.cmd hello.cmd menu.dat menu1.dat menu2.dat batch.sub nest.sub soak/*
	cp cpmbase.img cpmtest.img
	cpmcp -f ibmpc-514ss cpmtest.img menu.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img msub.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img hello.cmd 0:
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
DIST_CMD  = menu.cmd msub.cmd hello.cmd
DIST_DAT  = menu.dat menu1.dat menu2.dat soak/soak.dat
DIST_SUB  = batch.sub nest.sub soak/soak.sub soak/soaka.sub soak/soakb.sub soak/soakm.sub
DIST_DOC  = README.md LICENSE.md

dist: msub.zip

msub.zip: $(DIST_CMD) $(DIST_DAT) $(DIST_SUB) $(DIST_DOC)
	rm -f msub.zip
	zip -j msub.zip $^

# --------------------------------------------------------------------
# Utility
# --------------------------------------------------------------------
clean:
	$(RM) cpmtest.img *.o *.lib menu.cmd msub.cmd hello.cmd msub.zip

test: cpm86test

cpm86test: cpmtest.img
	./cpm86
