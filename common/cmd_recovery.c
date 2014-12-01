#include <common.h>
#include <command.h>

int recovery_mode_flag = 0;

int do_recovery(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[]);

void check_recovery_mode(void)
{
    if (recovery_mode_flag) {
        do_recovery(NULL, 0, 0, NULL);
    }
}

int do_recovery(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	int ret = 1;

	printf("setup env for recovery..\n");
#ifdef CONFIG_SYS_USE_NANDFLASH
    setenv("bootcmd", CONFIG_ANDROID_RECOVERY_BOOTCMD_NAND);
#elif CONFIG_SYS_USE_MMC
    setenv("bootcmd", CONFIG_ANDROID_RECOVERY_BOOTCMD_MMC);
#endif

	return ret;
}

U_BOOT_CMD(
	recovery,	1,	1,	do_recovery,
	"setup env for android recovery mode",
	""
);

