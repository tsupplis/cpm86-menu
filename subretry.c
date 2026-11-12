/* Caller-side recovery: never return to the CCP with an unrolled batch.
 * Direct console I/O bypasses redirection and does not interpret Ctrl-C. */
#include <stdio.h>
#include "subretry.h"

extern int bdos(int fn, int dx);

void sub_retry_wait(void)
{
    char message[160];
    char *p;
    int key;
    sprintf(message,"\r\nSubmit rollback failed in user %d drive %c.\r\n"
            "Queued commands may remain in $$$.SUB. Repair disk; press R to retry.\r\n",
            bdos(32,255)&255, 'A'+(bdos(25,0)&15));
    for(p=message;*p;p++) bdos(6,*p);
    do { key=bdos(6,255)&255; } while(key!='r' && key!='R');
}
