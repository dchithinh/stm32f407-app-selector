#include "sd_card_test.h"
#include "ff.h"
#include "sd_spi.h"
#include <stdio.h>
#include "main.h"

static const char *fs_type_name(BYTE t)
{
	switch (t) {
	case FS_FAT12: return "FAT12";
	case FS_FAT16: return "FAT16";
	case FS_FAT32: return "FAT32";
	case FS_EXFAT: return "exFAT";
	default: return "unknown";
	}
}

static void list_dir(const char *path)
{
	DIR dir;
	FILINFO fno;
	FRESULT fr;

	fr = f_opendir(&dir, path);
	if (fr != FR_OK) {
		printf("f_opendir(%s) failed: %d\r\n", path, fr);
		return;
	}

	printf("--- %s ---\r\n", path);
	for (;;) {
		fr = f_readdir(&dir, &fno);
		if (fr != FR_OK || fno.fname[0] == 0) {
			break;
		}
		if (fno.fattrib & AM_DIR) {
			printf("  [DIR]  %s\r\n", fno.fname);
		} else {
			printf("  %8lu  %s\r\n", (unsigned long)fno.fsize, fno.fname);
		}
	}
	f_closedir(&dir);
}

bool sd_card_test_run(void)
{
	FATFS fs;
	FRESULT fr;
	DWORD free_clusters;
	FATFS *pfs = &fs;

	sd_spi_init();
	HAL_Delay(50);

	fr = f_mount(&fs, "", 1);
	if (fr != FR_OK) {
		printf("SD mount failed: %d\r\n", fr);
		return false;
	}

	printf("SD mounted OK (%s)\r\n", fs_type_name(fs.fs_type));

	if (f_getfree("", &free_clusters, &pfs) == FR_OK) {
		uint64_t total_bytes = (uint64_t)(fs.n_fatent - 2) * fs.csize * 512ULL;
		uint64_t free_bytes = (uint64_t)free_clusters * fs.csize * 512ULL;
		printf("Volume: %llu KB total, %llu KB free\r\n",
		       (unsigned long long)(total_bytes / 1024ULL),
		       (unsigned long long)(free_bytes / 1024ULL));
	}

	list_dir("/");
	list_dir("/apps");

	return true;
}
