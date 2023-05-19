#include "mbed.h"
#include "SPIFBlockDevice.h"

#ifndef POST_APPLICATION_ADDR
#error "target.restrict_size must be set for your target in mbed_app.json"
#endif
#ifndef MBED_CONF_APP_FLASH_SIZE
#error "flash_size must be set inside config in mbed_app.json"
#endif
#ifndef MBED_CONF_APP_BOOTLOADER_SIZE
#error "bootloader_size must be set inside config in mbed_app.json"
#endif

SPIFBlockDevice spif;
FlashIAP flash;

const bd_size_t mcu_flash_size = MBED_CONF_APP_FLASH_SIZE;
const bd_size_t bootloader_size = MBED_CONF_APP_BOOTLOADER_SIZE;
const bd_size_t fw_max_size = mcu_flash_size - bootloader_size;
bd_size_t spif_size = 0;
bd_size_t spif_erase_size = 0;

// version format => 20230101_010101
const uint8_t fw_version_len = 15;
bd_addr_t current_fw_version_addr;
bd_addr_t dl_fw_version_addr;
bd_addr_t fw_addr = 0;

char current_fw_version[fw_version_len + 1] = {'\0'};
char dl_fw_version[fw_version_len + 1] = {'\0'};

// Firmware update related functions
bool fw_update_available();
void perform_fw_update();

// Main function
int main() {
    ThisThread::sleep_for(100ms);
    printf("\r\n\r");
    printf("----------------------\r\n");
    printf("Bootloader running...\r\n");
    printf("----------------------\r\n");

    // we initialize spi flash here, it's used both in check and update fw functions
    // we initialize mcu flash only on update, speeds up app start if update not needed

    spif.init();

    spif_size = spif.size();
    spif_erase_size = spif.get_erase_size();

    if (fw_update_available()) {
        perform_fw_update();
    }

    spif.deinit();

#ifdef POST_APPLICATION_ADDR
    printf("Starting main application\r\n\r\n");
    mbed_start_application(POST_APPLICATION_ADDR);
#endif
}

/**
 * Reads current and latest firmware versions from SPI.
 * Compares versions and determines if firmware update is available.
 *
 * When SPI flash is new or erased (usually all bytes are 0xFF), versions will be "same", so update will not be performed.
 * In main application, firmware is downloaded, stored in SPI flash and latest firmware version is set.
 * Then on next device restart, update will be performed.
 *
 * @note
 *   Firmware versions are in format YYYYmmdd_HHMMSS
 *
 * @returns
 *   A boolean value representing if firmware update is available
 */
bool fw_update_available() {

    // spif has to be initialized for the check
    if (spif_size == 0 || spif_erase_size == 0) return false;

    if (fw_max_size % spif_erase_size != 0) {
        fw_addr = spif_size - (((fw_max_size / spif_erase_size)+1) * spif_erase_size);
    } else {
        fw_addr = spif_size - fw_max_size;
    }
    dl_fw_version_addr = fw_addr - spif_erase_size;
    current_fw_version_addr = dl_fw_version_addr - spif_erase_size;
    /* print spif related stuff - debug only
    printf("FW size: 0x%.8llx\r\n", fw_max_size);
    printf("SPIF size: 0x%.8llx\r\n", spif_size);
    printf("FW addr: 0x%.8llx\r\n", fw_addr);
    printf("Downloaded FW version addr: 0x%.8llx\r\n", dl_fw_version_addr);
    printf("Current FW version addr: 0x%.8llx\r\n", current_fw_version_addr);
    printf("----------------------\r\n"); //*/

    /* force update - debug only
    spif.erase(current_fw_version_addr, spif_erase_size);
    spif.program("123456789456123", current_fw_version_addr, fw_version_len); //*/

    /* erase spi flash - debug only
    int erase_block = (256 * 1024) + (spif_erase_size * 2);
    spif.erase(spif_size - erase_block, erase_block); //*/

    // immediately after flashing device, both current_fw_version and dl_fw_version will be erased (all 1s)
    // read current_fw_version from spif
    spif.read(current_fw_version, current_fw_version_addr, fw_version_len);
    // read dl_fw_version from spif
    spif.read(dl_fw_version, dl_fw_version_addr, fw_version_len);

    /* print fw versions - debug only
    printf("Current firmware version (");
    for(int i=0; i<fw_version_len; i++) printf("%c", current_fw_version[i] >= 32 ? current_fw_version[i] : ' ');
    printf(") <=> (");
    for(int i=0; i<fw_version_len; i++) printf("%c", dl_fw_version[i] >= 32 ? dl_fw_version[i] : ' ');
    printf(") Latest firmware version\r\n"); //*/

    if (strncmp(current_fw_version, dl_fw_version, fw_version_len) != 0) {
        printf("Firmware update available\r\n");
        return true;
    }
    printf("Firmware is up to date\r\n");
    return false;
}

/**
 * Performs firmware update.
 *
 * Reads page_size bytes from SPI flash, writes them to MCU flash, then moves on to next page until fw_max_size bytes are written.
 * After flashing firmware, sets the current firmware version to the latest firmware version.
 *
 * @note
 *   page_size represents MCU flash write page size.
*/
void perform_fw_update() {

    flash.init();

    const uint32_t flash_start = flash.get_flash_start();
    const uint32_t flash_size = flash.get_flash_size();
    const uint32_t page_size = flash.get_page_size();
    uint8_t *page_buffer = new uint8_t[page_size];

    bd_addr_t spif_start_addr = fw_addr;
    bd_addr_t spif_end_addr = fw_addr + fw_max_size;
    uint32_t mcu_flash_start_addr = POST_APPLICATION_ADDR;
    uint32_t mcu_flash_end_addr = flash_start + flash_size;

    uint32_t next_sector_addr = mcu_flash_start_addr + flash.get_sector_size(mcu_flash_start_addr);
    bool sector_erased = false;
    uint32_t pages_total = fw_max_size / page_size;
    uint32_t pages_flashed = 0;
    uint32_t percent_done = 0;

    bd_size_t index = 0;
    bool error = false;
    char* err_msg = new char[1] {0x00};

    while (true) {

        if (index >= fw_max_size) break;

        if (spif_start_addr + index + page_size > spif_end_addr) {
            err_msg = (char*) malloc(sizeof(char) * 100);
            sprintf(err_msg, "SPI flash out of bounds reached. aborting!\r\nSPI flash addr: %llx\r\n", spif_start_addr + index);
            error = true;
            break;
        }
        if (mcu_flash_start_addr + index + page_size > mcu_flash_end_addr) {
            err_msg = (char*) malloc(sizeof(char) * 100);
            sprintf(err_msg, "MCU flash out of bounds reached. aborting!\r\nMCU flash addr: %llx\r\n", mcu_flash_start_addr + index);
            error = true;
            break;
        }

        // Read data for this page
        memset(page_buffer, 0xff, sizeof(uint8_t) * page_size);
        spif.read(page_buffer, spif_start_addr + index, page_size);

        // Erase this sector if it hasn't been erased
        if (!sector_erased) {
            flash.erase(mcu_flash_start_addr + index, flash.get_sector_size(mcu_flash_start_addr + index));
            sector_erased = true;
        }

        // Program page
        int result = flash.program(page_buffer, mcu_flash_start_addr + index, page_size);
        if (result != 0)
        {
            err_msg = (char*) malloc(sizeof(char) * 50);
            sprintf(err_msg, "Firmware update failed: %llx %d\r\n", mcu_flash_start_addr + index, page_size);
            error = true;
            break;
        }

        pages_flashed++;

        index += page_size;
        if (mcu_flash_start_addr + index >= next_sector_addr) {
            next_sector_addr = mcu_flash_start_addr + index + flash.get_sector_size(mcu_flash_start_addr + index);
            sector_erased = false;
        }

        percent_done = pages_flashed * 100 / pages_total;
        if (percent_done % 5 == 0) {
            printf("Flashed %3d%%\r", percent_done);
        }
    }

    if (error) {
        printf("\nFirmware update unsuccessful\r\n");
        printf("%s", err_msg);
    } else {
        printf("\nFirmware update successful\r\n");
        spif.erase(current_fw_version_addr, spif_erase_size);
        spif.program(dl_fw_version, current_fw_version_addr, fw_version_len);
    }

    free(err_msg);
    delete[] page_buffer;

    flash.deinit();
}
