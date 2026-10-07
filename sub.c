#include "sub.h"

/*
 * FCB for $$$     .SUB
 * Byte  0   : drive (0 = default)
 * Bytes 1-8 : filename, space-padded
 * Bytes 9-11: extension
 * Bytes 12-31: reserved (zeroed)
 * Byte 32   : current record (cr), auto-advanced by BDOS 21
 */
static char _fcb[33] = {
    0,                                  /* drive: default */
    '$','$','$',' ',' ',' ',' ',' ',   /* name: "$$$     " */
    'S','U','B',                        /* ext:  "SUB" */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  /* bytes 12-30 */
    0                                   /* cr (byte 32) */
};

static char _buf[128]; /* sector work buffer */

int sub_open(int flags)
{
    int i;

    if (flags == SUB_APPEND)
        return -1;  /* stub: not implemented in v1 */

    /* reset FCB current-record counter */
    _fcb[32] = 0;

    /* BDOS 19: delete (ignore error -- file may not exist) */
    bdos(19, _fcb);

    /* BDOS 22: make (create) */
    if ((bdos(22, _fcb) & 0xFF) == 0xFF)
        return -1;

    return 0;
}

int sub_append(char *cmd)
{
    int i;
    int len;
    char c;

    /* zero the sector buffer */
    for (i = 0; i < 128; i++)
        _buf[i] = 0;

    /* measure and upcase-copy the command */
    len = 0;
    while (cmd[len] != '\0' && len < 125) {
        c = cmd[len];
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
        _buf[1 + len] = c;
        len++;
    }

    /* sector layout: [0]=length, [1..len]=cmd, [len+1]=0x00, [len+2]='$' */
    _buf[0]       = (char)len;
    _buf[1 + len] = 0x00;
    _buf[2 + len] = '$';

    /* BDOS 26: set DMA address to _buf */
    bdos(26, _buf);

    /* BDOS 21: sequential write */
    if ((bdos(21, _fcb) & 0xFF) != 0)
        return -1;

    return 0;
}

int sub_close(void)
{
    /* BDOS 16: close file */
    if ((bdos(16, _fcb) & 0xFF) == 0xFF)
        return -1;

    return 0;
}

int sub_delete(void)
{
    /* BDOS 19: delete -- ignore error if file does not exist */
    bdos(19, _fcb);
    return 0;
}
