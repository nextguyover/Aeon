#ifndef SD_H
#define SD_H

#include <stdbool.h>
#include <stdint.h>

#include "ff.h"

bool sd_init(FATFS* FatFs);
void sd_close();
void sd_write_logfile();
bool sd_get_next_image_filename(char* filename_buf, bool shuffle_enabled);
void sd_append_batt_charge_log(int sleep_seconds);

#endif  // SD_H
