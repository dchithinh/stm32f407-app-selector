#ifndef BSP_SD_APPS_H
#define BSP_SD_APPS_H

#include <stdint.h>
#include <stdbool.h>

#define SD_APPS_MAX           12U
#define SD_APPS_DIR           "/apps"
#define SD_APP_NAME_LEN       28U
#define SD_APP_PATH_LEN       48U

typedef struct {
	char name[SD_APP_NAME_LEN];
	char path[SD_APP_PATH_LEN];
	uint32_t size;
} sd_app_entry_t;

typedef struct {
	sd_app_entry_t items[SD_APPS_MAX];
	uint8_t count;
	char error[64];
} sd_app_list_t;

bool sd_apps_mount(void);
void sd_apps_unmount(void);
bool sd_apps_scan(sd_app_list_t *out);

#endif
