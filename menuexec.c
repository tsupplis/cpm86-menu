/* Execution policy shared by the two MENU backends. */
#include <stdio.h>
#include "util.h"
#include "sub.h"
#include "menuexec.h"

char *menu_exec_init(menu_exec_t *ctx)
{
    int version;

    ctx->error_arg = 0;
    ctx->command[0] = '\0';
    version = bdos(12, 0) & 0xFF;
    if (version == 0x22) {
        ctx->mode = MEXEC_SUB;
        ctx->cmddrv = sub_cmddrv();
        ctx->subact = sub_active();
        return 0;
    }
    /* Match util.lib's MP/M / concurrent system-type gate, as well as
       checking the version. A later single-tasking BDOS is insufficient. */
    if (version >= 0x30 && (ostype() & 0x05)) {
        ctx->mode = MEXEC_CLI;
        return 0;
    }
    return "CP/M-86 1.1 or Concurrent CP/M-86 BDOS >= 3.0 required";
}

char *menu_exec_prepare(menu_exec_t *ctx, menu_item_t *item, char *datfile, int sel)
{
    ctx->error_arg = 0;
    if (ctx->mode == MEXEC_CLI)
        return menuexe_prepare(ctx, item);
    return menusub_prepare(ctx, item, datfile, sel);
}

int menu_exec_run(menu_exec_t *ctx, menu_item_t *item)
{
    if (ctx->mode == MEXEC_CLI)
        return menuexe_run(ctx, item);
    menusub_run(ctx, item);
    return MEXEC_EXIT;
}

void menu_exec_quit(menu_exec_t *ctx)
{
    /* On CP/M preserve an enclosing job; discard a stale keyboard stack.
       Concurrent MENU never inspects or changes $$$.SUB. */
    if (ctx->mode == MEXEC_SUB && ctx->subact != 1)
        sub_delete();
}
