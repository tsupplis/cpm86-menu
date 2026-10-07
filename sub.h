#ifndef SUB_H
#define SUB_H

#define SUB_CREATE  0   /* delete any existing $$$SUB, start fresh          */
#define SUB_APPEND  1   /* push on top of an existing $$$SUB, else create   */

int sub_open(int flags);
int sub_append(char *cmd);
int sub_close(void);
int sub_abort(void);   /* undo since sub_open: delete if created, else restore rc */
int sub_delete(void);  /* BDOS 19: delete $$$SUB if it exists */
void sub_exit(void);       /* BDOS fn 0: warm boot, CCP picks up $$$SUB automatically */
void p_chain(char *cmd, int submode); /* BDOS 47: chain; submode!=0 sets MDSUBE so $$$SUB runs after */
int  sub_cmddrv(void);     /* drive (1=A..16=P) prefixing the running CCP command, 0 = none/unknown */
int  sub_active(void);     /* CCP submit mode: 1 = on, 0 = off, -1 = unknown CCP */

#endif
