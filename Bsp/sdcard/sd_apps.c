#include "sd_apps.h"
#include "boot_layout.h"
#include "ff.h"
#include "sd_spi.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

static FATFS g_fs;
static bool g_mounted;

static bool ends_with_bin(const char *name)
{
	size_t n = strlen(name);

	if (n < 4U) {
		return false;
	}
	return (name[n - 4U] == '.') &&
	       (name[n - 3U] == 'b' || name[n - 3U] == 'B') &&
	       (name[n - 2U] == 'i' || name[n - 2U] == 'I') &&
	       (name[n - 1U] == 'n' || name[n - 1U] == 'N');
}

static void strip_bin_ext(char *dst, size_t dst_len, const char *fname)
{
	size_t n = strlen(fname);
	size_t copy = n;

	if (copy >= 4U && ends_with_bin(fname)) {
		copy -= 4U;
	}
	if (copy >= dst_len) {
		copy = dst_len - 1U;
	}
	memcpy(dst, fname, copy);
	dst[copy] = '\0';
}

bool sd_apps_mount(void)
{
	FRESULT fr;

	if (g_mounted) {
		return true;
	}

	sd_spi_init();
	HAL_Delay(100);

	fr = f_mount(&g_fs, "", 1);
	if (fr != FR_OK) {
		return false;
	}
	g_mounted = true;
	return true;
}

void sd_apps_unmount(void)
{
	if (g_mounted) {
		(void)f_mount(NULL, "", 0);
		g_mounted = false;
	}
}

bool sd_apps_scan(sd_app_list_t *out)
{
	DIR dir;
	FILINFO fno;
	FRESULT fr;

	memset(out, 0, sizeof(*out));

	if (!g_mounted && !sd_apps_mount()) {
		snprintf(out->error, sizeof(out->error), "SD mount failed");
		return false;
	}

	fr = f_opendir(&dir, SD_APPS_DIR);
	if (fr != FR_OK) {
		snprintf(out->error, sizeof(out->error), "No %s folder", SD_APPS_DIR);
		return false;
	}

	for (;;) {
		fr = f_readdir(&dir, &fno);
		if (fr != FR_OK || fno.fname[0] == 0) {
			break;
		}
		if (fno.fattrib & AM_DIR) {
			continue;
		}
		if (!ends_with_bin(fno.fname)) {
			continue;
		}
		if (fno.fsize == 0U || fno.fsize > APP_FLASH_SIZE) {
			continue;
		}
		if (out->count >= SD_APPS_MAX) {
			break;
		}

		{
			sd_app_entry_t *e = &out->items[out->count];

			snprintf(e->path, sizeof(e->path), "%s/%s", SD_APPS_DIR, fno.fname);
			strip_bin_ext(e->name, sizeof(e->name), fno.fname);
			e->size = fno.fsize;
			out->count++;
		}
	}
	f_closedir(&dir);

	if (out->count == 0U) {
		snprintf(out->error, sizeof(out->error), "No .bin in %s", SD_APPS_DIR);
		return false;
	}
	return true;
}
