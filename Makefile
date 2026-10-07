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
cpmtest.img: menu.cmd msub.cmd hello.cmd menu.dat menu1.dat menu2.dat batch.sub nest.sub
	cp cpmbase.img cpmtest.img
	cpmcp -f ibmpc-514ss cpmtest.img menu.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img msub.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img hello.cmd 0:
	cpmcp -f ibmpc-514ss cpmtest.img *.dat 0:
	cpmcp -f ibmpc-514ss cpmtest.img batch.sub 0:
	cpmcp -f ibmpc-514ss cpmtest.img nest.sub 0:
	cpmls -F -f ibmpc-514ss cpmtest.img 

# --------------------------------------------------------------------
# Binary zip
# --------------------------------------------------------------------
dist: menu-bin.zip

menu-bin.zip: menu.cmd
	rm -f menu-bin.zip
	zip menu-bin.zip menu.cmd

# --------------------------------------------------------------------
# Utility
# --------------------------------------------------------------------
clean:
	$(RM) cpmtest.img *.o *.lib menu.cmd msub.cmd hello.cmd

test: cpm86test

cpm86test: cpmtest.img
	./cpm86
