/* Resident Concurrent CP/M execution. No CCP stack or MENU restart. */
#include <stdio.h>
#include "util.h"
#include "menuexec.h"

char *menuexe_prepare(menu_exec_t *ctx, menu_item_t *item)
{
    char *prefix;
    int i;
    int n;
    int parent;

    prefix = (item->type == MTYPE_S || item->type == MTYPE_SNR) ? "SUBMIT " : "";
    n = 0;
    for (i = 0; prefix[i]; i++) ctx->command[n++] = prefix[i];
    for (i = 0; item->cmd[i]; i++) {
        if (n == MEXEC_CMDLEN)
            return "Command too long for P_CLI";
        ctx->command[n++] = item->cmd[i];
    }
    ctx->command[n] = '\0';

    ctx->priority = getpriority();
    parent = getppriority();
    /* Like native SUBMIT, run ahead of the parent to recover the console.
       Avoid competing with system processes (priorities below 64). */
    if (ctx->priority < 0 || ctx->priority > 255 || parent <= 64 || parent > 255)
        return "Cannot determine safe console priority";
    ctx->wait_priority = parent - 1;
    /* TMP normally runs at 198. Even with a lower-priority immediate
       parent, reclaim the console before TMP can take it (util.h). */
    if (ctx->wait_priority > 197) ctx->wait_priority = 197;
    return 0;
}

int menuexe_run(menu_exec_t *ctx, menu_item_t *item)
{
    int home;
    int drive;
    int rc;

    home = bdos(32, 0xFF) & 0xFF;
    drive = bdos(25, 0) & 0xFF;
    if (item->user >= 0) bdos(32, item->user);

    /* Establish priority before launch: a nested SUBMIT derives its own
       priority from ours. Do no console I/O until C_ATTACH has returned. */
    p_priority(ctx->wait_priority);
    rc = p_cli(ctx->command);
    if (rc == 0) conattach();
    p_priority(ctx->priority);

    /* USER and $GLOBAL in native SUBMIT can change the parent's context.
       Restore even when there was no @n, or the launch failed. */
    bdos(32, home);
    bdos(14, drive);
    if (rc != 0) return MEXEC_ERROR;
    if (item->type == MTYPE_ENR || item->type == MTYPE_SNR || item->type == MTYPE_CNR)
        return MEXEC_EXIT;
    return MEXEC_RESUME;
}
