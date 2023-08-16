/* 
    SD card and FAT filesystem 
    This example uses SPI peripheral to communicate with SD card.
*/

#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#define EXAMPLE_MAX_CHAR_SIZE    64

static const char *TAG = "example";

#define MOUNT_POINT "/sdcard"

// Pin assignments SD
#define PIN_NUM_MISO 37
#define PIN_NUM_MOSI 35
#define PIN_NUM_CLK 36
#define PIN_NUM_CS 38

static esp_err_t s_example_write_file(const char *path, char *data)
{
    ESP_LOGI(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return ESP_FAIL;
    }
    fprintf(f, data);
    fclose(f);
    ESP_LOGI(TAG, "File written");

    return ESP_OK;
}

static esp_err_t s_example_read_file(const char *path)
{
    ESP_LOGI(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return ESP_FAIL;
    }
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // strip newline
    char *pos = strchr(line, '\n');
    if (pos) {
        *pos = '\0';
    }
    ESP_LOGI(TAG, "Read from file: '%s'", line);

    return ESP_OK;
}

void app_main(void)
{
    esp_err_t ret;

    // Options for mounting the filesystem.
    // If format_if_mount_failed is set to true, SD card will be partitioned and
    // formatted in case when mounting fails.
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    sdmmc_card_t *card;
    const char mount_point[] = MOUNT_POINT;
    ESP_LOGI(TAG, "Initializing SD card");

    // Use settings defined above to initialize SD card and mount FAT filesystem.
    // Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
    // Please check its source code and implement error recovery when developing
    // production applications.
    ESP_LOGI(TAG, "Using SPI peripheral");

    // By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
    // For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
    // Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize bus.");
        return;
    }

    // This initializes the slot without card detect (CD) and write protect (WP) signals.
    // Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = host.slot;

    ESP_LOGI(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount filesystem. "
                     "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        } else {
            ESP_LOGE(TAG, "Failed to initialize the card (%s). "
                     "Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "Filesystem mounted");

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files.
//__________________________________________Code for datalogger________________________________________________________
    // Create blackup file
    const char *BACKUP_DATA = MOUNT_POINT "/backup.txt";
    FILE *backup_data = fopen(BACKUP_DATA, "w");
    if (backup_data == NULL)
    {
        ESP_LOGE(TAG, "Failed to open backup file for writing");
    }
    //___________________________________________Read error_____________________________________________________________
    const char *FILE_FAILED = MOUNT_POINT "/failed.txt";
    FILE *file_failed = fopen(FILE_FAILED, "r");
    if (file_failed == NULL)
    {
        ESP_LOGI(TAG, "data file does not exist, creating...");
    }
    else
    {
        ESP_LOGI(TAG, "Data file exists, creating backup");
        char c;
        while (!feof(file_failed))
        {
            c = fgetc(file_failed);
            fputc(c, backup_data);
            printf("%c", c);
        }
        fclose(file_failed);
    }
    //____________________________________________Write error__________________________________________________________ 
    file_failed = fopen(FILE_FAILED, "a");
    if (file_failed == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return;
    }
    //_____________________________________________Data writing _______________________________________________________
    float temperature = 20.0 + (float)(rand() % 10);
    float pressure = 5.0 + (float)(rand() % 20);
    char timestamp[30];
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    fprintf(file_failed, "%s,%.2f,%.2f\n", timestamp, temperature, pressure);
    fclose(file_failed);
    fclose(backup_data);
    remove(BACKUP_DATA); // Remove backup data
    //____________________All done, unmount partition and disable SPI peripheral_______________________________________
    esp_vfs_fat_sdcard_unmount(mount_point, card);
    ESP_LOGI(TAG, "Card unmounted");

    //deinitialize the bus after all devices are removed
    spi_bus_free(host.slot);
}
