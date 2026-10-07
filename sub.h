#ifndef SUB_H
#define SUB_H

#define SUB_CREATE  0   /* delete any existing $$$SUB, start fresh */
#define SUB_APPEND  1   /* open existing $$$SUB, append (stub in v1) */

int sub_open(int flags);
int sub_append(char *cmd);
int sub_close(void);
int sub_delete(void);  /* BDOS 19: delete $$$SUB if it exists */
void sub_exit(void);       /* BDOS fn 0: warm boot, CCP picks up $$$SUB automatically */
void p_chain(char *cmd, int submode); /* BDOS 47: chain; submode!=0 sets MDSUBE so $$$SUB runs after */

#endif
