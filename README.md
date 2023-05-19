# Mbed OS FOTA update bootloader

***DISCLAIMER:** Firmware-Over-The-Air (FOTA) updates are delicate matter and proper security measures need to be taken to securely utilize FOTA updates in production. This project is done as a proof of concept, no security measures were taken and I will not be held responsible if you use this in production and a malicious third party gains access to your device. **YOU HAVE BEEN WARNED**. That being said, this project has been tested and is fully working.*

Useful information for using custom bootloader on Mbed OS is available [here](https://os.mbed.com/docs/mbed-os/v6.16/program-setup/creating-and-using-a-bootloader.html) and [here](https://os.mbed.com/docs/mbed-os/v6.16/program-setup/bootloader-configuration.html).

This Mbed OS project is used for building a bootloader for utilizing FOTA updates on Mbed OS supported board. To keep bootloader size minimal, bootloader does not connect or download anything, only checks if firmware update is available and if so, copies the firmware from SPI flash to MCU flash.

Main application is responsible for connecting to internet, downloading firmware update, storing it in SPI flash and restarting device so bootloader will flash the new firmware.
Mbed OS project for the main application is published to a [separate repository](https://github.com/kiko283/mbed-os-fota-main-application).

For this project, bootloader size is ~25kB, so bootloader ***mbed_app.json*** has the following:

    ...
    "target_overrides": {
        "NUCLEO_L433RC_P": {
            "target.restrict_size": "0x8000"
        }
    }
    ...

`0x8000 (hex) = 32768 (dec) = 32kB`

Also, because we will be communicating with external SPI flash chip, we need to add few things to make it work:

    ...
    "requires": ["spif-driver"],
    "target_overrides": {
        "NUCLEO_L433RC_P": {
            "target.components_add": ["SPIF"],
            "spif-driver.SPI_MOSI": "PA_7",
            "spif-driver.SPI_MISO": "PA_6",
            "spif-driver.SPI_CLK":  "PA_5",
            "spif-driver.SPI_CS":   "PA_4"
        }
    }
    ...

Development environment:

1. IDE: [Mbed Studio](https://os.mbed.com/studio/)
    * To reduce size when building the project, use *Release* profile in Mbed Studio.

1. Development board: [NUCLEO-L433RC-P](https://os.mbed.com/platforms/NUCLEO-L433RC-P/)
    * MCU: [STM32L433RC](https://www.st.com/en/microcontrollers-microprocessors/stm32l433rc.html)
    * MCU flash size: 256kB
    * MCU SRAM size: 64kB

1. Connectivity: [SparkFun LTE CAT M1/NB-IoT Shield - SARA-R4](https://www.sparkfun.com/products/14997)
    * cellular modem: [ublox SARA-R410M (SARA-R410M-02B-01)](https://content.u-blox.com/sites/default/files/SARA-R4_DataSheet_UBX-16024152.pdf)
    * ***NOTE1**: I replaced the cellular modem with **ublox SARA-R412M (SARA-R412M-02B-01)** in order to connect to 2G network, no LTE Cat M1/M2 or NB-IoT networks are available locally.*
    * ***NOTE2**: both SARA-R410M and SARA-R412M have **end of life** status, so SARA-R422 / SARA-R422S / SARA-R422M10S should be used instead.*
    * ***NOTE3**: Different type of connectivity can be used as well (different cellular modem, ethernet connection, Wi-Fi), requirement for this project was to be battery powered and with cellular connection. if different connectivity method is used, appropriate Mbed OS network stack will be needed.*

1. External SPI flash chip: [W25Q16JVSIQ](https://www.winbond.com/hq/product/code-storage-flash-memory/serial-nor-flash/?__locale=en&partNo=W25Q16JV)
    * SPI flash size: 16Mb (2MB)
    * *The SPI flash chip I chose is actually W25Q128JVSIQ (same family, larger size), but this was available locally during development.*

**NOTE: if using different MCU / development board, make sure the internal MCU flash size is 256kB or greater, the code in this FOTA example is not optimized for MCUs with less storage.**

Support
======

<a href="https://www.buymeacoffee.com/kiko283" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-white.png" alt="Buy Me A Coffee" style="height: 60px !important;width: 217px !important;" ></a>
