#include "sd.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "aeon.h"
#include "ff.h"
#include "fram.h"
#include "main.h"

/**
 * @brief Initialise the SD card and mount the filesystem.
 *
 * @param FatFs
 */
bool sd_init(FATFS* FatFs) {
    FRESULT fres;
    fres = f_mount(FatFs, "", 1);  // 1=mount now
    if (fres != FR_OK) {
        printf("f_mount error (%i)\r\n", fres);
        return false;
    }
    return true;
}

/**
 * @brief Unmount the filesystem.
 */
void sd_close() { f_mount(NULL, "", 0); }

/**
 * @brief Write the debug output buffer to a logfile on the SD card.
 */
void sd_write_logfile() {
    FIL fil;
    FRESULT fres;

    fres = f_mkdir("/logfiles");
    if (fres != FR_OK && fres != FR_EXIST) return;

    fres = f_chdir("/logfiles");
    if (fres != FR_OK) return;

    char filename[41] = {0};
    sprintf(filename, "log_%lu.txt", wake_cycle_count);

    fres = f_open(&fil, filename, FA_WRITE | FA_OPEN_ALWAYS | FA_OPEN_APPEND);
    if (fres != FR_OK) return;

    UINT bytesWrote;
    fres = f_write(&fil, debug_output_arr, debug_output_arr_index, &bytesWrote);
    if (fres != FR_OK) return;

    fres = f_close(&fil);
    if (fres != FR_OK) return;

    f_chdir("/");
}

/**
 * @brief Log battery charge information to a CSV file.
 */
void sd_append_batt_charge_log(int sleep_seconds) {
    FIL fil;
    FRESULT fres;
    UINT bytesWrote;

    fres = f_mkdir("/logfiles");
    if (fres != FR_OK && fres != FR_EXIST) return;

    // Check if the file exists
    fres = f_stat("/logfiles/batt-charge.csv", NULL);
    bool file_exists = (fres == FR_OK);

    // Open the file for appending, create if it doesn't exist
    fres = f_open(&fil, "/logfiles/batt-charge.csv", FA_WRITE | FA_OPEN_APPEND);
    if (fres != FR_OK) return;

    // If the file was just created, write the header
    if (!file_exists) {
        const char* header =
            "BootIteration,BatteryVoltage,WakeReason,SleepReason,"
            "SleepDurationSeconds\r\n";
        fres = f_write(&fil, header, strlen(header), &bytesWrote);
        if (fres != FR_OK) {
            f_close(&fil);
            return;
        }
    }

    enum sleep_reason_t sleep_reason = fram_get_sleep_reason();
    const char* sleep_reason_str = sleep_reason_t_str[sleep_reason];

    const char* wake_reason_str = wake_reason_t_str[wake_reason];

    // Prepare the log entry
    char log_entry[100];
    snprintf(log_entry, sizeof(log_entry), "%lu,%.2f,%s,%s,%d\r\n",
             wake_cycle_count, batt_voltage, wake_reason_str, sleep_reason_str,
             sleep_seconds);

    // Write the log entry to the file
    fres = f_write(&fil, log_entry, strlen(log_entry), &bytesWrote);
    f_close(&fil);
}

bool sd_get_next_image_filename(char* filename_buf, bool shuffle_enabled) {
    FRESULT fres;

    fres = f_chdir("/images");
    if (fres != FR_OK) return false;

    uint32_t img_counter = fram_get_image_counter();
    if (!shuffle_enabled) {
        sprintf(filename_buf, "%lu.slc", img_counter);

        fres = f_stat(filename_buf, NULL);

        if (fres == FR_OK) {
            fram_set_image_counter(img_counter + 1);
            return true;
        } else if (fres == FR_NO_FILE) {
            // if file does not exist, reset counter
            fram_set_image_counter(1);
            memcpy(filename_buf, "0.slc", 6);
        } else {
            return false;
        }

        // double check that zeroth image exists
        fres = f_stat(filename_buf, NULL);
        return fres == FR_OK;
    } else {
        DIR dir;
        FILINFO fno;
        uint32_t count = 0;
        FRESULT res;

        // count all files in the current `/images` directory
        res = f_opendir(&dir, ".");
        if (res != FR_OK) return false;
        for (;;) {
            res = f_readdir(&dir, &fno);

            if (res != FR_OK || fno.fname[0] == '\0') break;

            // check that the entry is a file and not a directory
            if (!(fno.fattrib & AM_DIR)) {
                count++;  // increment file count
            }
        }
        f_closedir(&dir);
        if (count == 0) return false;

        uint32_t random;
        if (HAL_RNG_GenerateRandomNumber(&hrng, &random) != HAL_OK) {
            if (DBG) printf("Error generating random number\n");
            return false;
        }

        uint32_t index = random % count;  // select a random file index

        if (index == img_counter)  // if the random index is the same as the
                                   // current image index
            index = (index + 1) % count;  // skip the current image

        fram_set_image_counter(index);  // store the index

        // with known naming convention, directly generate filename
        sprintf(filename_buf, "%lu.slc", index);

        return true;
    }
}
