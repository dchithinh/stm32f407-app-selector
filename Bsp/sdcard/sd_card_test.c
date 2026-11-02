#include "sd_card_test.h"
#include "ff.h"
#include "sd_spi.h"
#include <stdio.h>
#include "main.h"

static const char *fr_str(FRESULT fr)
{
	switch (fr) {
	case FR_OK: return "FR_OK";
	case FR_DISK_ERR: return "FR_DISK_ERR";
	case FR_INT_ERR: return "FR_INT_ERR";
	case FR_NOT_READY: return "FR_NOT_READY (card missing or SPI/init failed)";
	case FR_NO_FILE: return "FR_NO_FILE";
	case FR_NO_PATH: return "FR_NO_PATH";
	case FR_INVALID_NAME: return "FR_INVALID_NAME";
	case FR_DENIED: return "FR_DENIED";
	case FR_EXIST: return "FR_EXIST";
	case FR_INVALID_OBJECT: return "FR_INVALID_OBJECT";
	case FR_WRITE_PROTECTED: return "FR_WRITE_PROTECTED";
	case FR_INVALID_DRIVE: return "FR_INVALID_DRIVE";
	case FR_NOT_ENABLED: return "FR_NOT_ENABLED";
	case FR_NO_FILESYSTEM: return "FR_NO_FILESYSTEM (format card FAT32)";
	case FR_MKFS_ABORTED: return "FR_MKFS_ABORTED";
	case FR_TIMEOUT: return "FR_TIMEOUT";
	default: return "FR_?";
	}
}

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
		printf("  (skip %s: %s)\r\n", path, fr_str(fr));
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

	printf("SD: mounting FatFs...\r\n");

	sd_spi_init();
	HAL_Delay(250);

	fr = f_mount(&fs, "", 1);
	if (fr != FR_OK) {
		printf("SD mount failed: %s (%d)\r\n", fr_str(fr), (int)fr);
		return false;
	}

	printf("SD mounted OK (%s)\r\n", fs_type_name(fs.fs_type));

	if (f_getfree("", &free_clusters, &pfs) == FR_OK) {
		uint64_t total_bytes = (uint64_t)(fs.n_fatent - 2) * fs.csize * 512ULL;
		uint64_t free_bytes = (uint64_t)free_clusters * fs.csize * 512ULL;
		/* newlib-nano has no %llu — print KB as unsigned long */
		printf("Volume: %lu KB total, %lu KB free\r\n",
		       (unsigned long)(total_bytes / 1024ULL),
		       (unsigned long)(free_bytes / 1024ULL));
	}

	list_dir("/");
	list_dir("/apps");

	return true;
}
