#ifndef BSP_TOUCH_BL_H
#define BSP_TOUCH_BL_H

#include <stdint.h>
#include <stdbool.h>

void touch_bl_init(void);
bool touch_bl_read(int16_t *x, int16_t *y, bool *pressed);
void touch_bl_probe(void);

#endif
