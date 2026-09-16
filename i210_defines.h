#ifndef _I210_DEFINES_H_
#define _I210_DEFINES_H_

#define I210_EERD          0x12014

#define EERD_START         BIT(0)
#define EERD_DONE          BIT(1)
#define EERD_ADDR_SHIFT    2
#define EERD_DATA_SHIFT    16

#define I210_TX_DESC_COUNT    64


#define DRV_NAME "i210"

/* Ethernet MAC address */
#define I210_NVM_MAC_ADDR_W0          0x00
#define I210_NVM_MAC_ADDR_W1          0x01
#define I210_NVM_MAC_ADDR_W2          0x02

/* Software compatibility */
#define I210_NVM_SW_COMPAT_HIGH_W0    0x03
#define I210_NVM_SW_COMPAT_LOW_W0     0x03

#define I210_NVM_SW_PORT_ID_LED_W     0x04
#define I210_NVM_SW_FLASH_MAP_REV_W   0x05

#define I210_NVM_SW_COMPAT_HIGH_W1    0x06
#define I210_NVM_SW_COMPAT_LOW_W1     0x06

#define I210_NVM_SW_COMPAT_HIGH_W2    0x07
#define I210_NVM_SW_COMPAT_LOW_W2     0x07

/* PBA */
#define I210_NVM_PBA_W0               0x08
#define I210_NVM_PBA_W1               0x09

/* Hardware initialization */
#define I210_NVM_INIT_CTRL_W1         0x0A
#define I210_NVM_SUBSYSTEM_ID         0x0B
#define I210_NVM_SUBSYSTEM_VENDOR_ID  0x0C
#define I210_NVM_DEVICE_ID            0x0D
#define I210_NVM_VENDOR_ID            0x0E
#define I210_NVM_INIT_CTRL_W2         0x0F

/* Firmware / Flash */
#define I210_NVM_SECURED_MODULE_PTR   0x10
#define I210_NVM_FLASH_DEVICE_SIZE    0x11
#define I210_NVM_FLASH_VALIDITY       0x12

/* Hardware initialization */
#define I210_NVM_INIT_CTRL_W4         0x13
#define I210_NVM_PCIE_L1_LATENCY      0x14
#define I210_NVM_PCIE_COMPLETION_TMO  0x15
#define I210_NVM_MSIX_CONFIG          0x16

/* Software reset CSR auto configuration */
#define I210_NVM_SW_RESET_CSR_PTR     0x17

/* PCIe initialization */
#define I210_NVM_PCIE_INIT_CFG_W1     0x18
#define I210_NVM_PCIE_INIT_CFG_W2     0x19
#define I210_NVM_PCIE_INIT_CFG_W3     0x1A
#define I210_NVM_PCIE_CTRL_W1         0x1B

/* LED / hardware configuration */
#define I210_NVM_LED1_CONFIG          0x1C
#define I210_NVM_DEVICE_REV_ID        0x1E
#define I210_NVM_LED0_2_CONFIG        0x1F
#define I210_NVM_SW_DEFINED_PINS      0x20
#define I210_NVM_FUNCTIONS_CTRL       0x21
#define I210_NVM_LAN_POWER_CONSUMPTION 0x22

/* PCIe reset CSR auto configuration */
#define I210_NVM_PCIE_RESET_CSR_PTR   0x23

/* Hardware initialization */
#define I210_NVM_INIT_CTRL_W3         0x24

/* mDNS */
#define I210_NVM_MDNS_RECORDS_OFFSET  0x25
#define I210_NVM_MDNS_RECORDS_SIZE    0x26

/* CSR auto configuration */
#define I210_NVM_CSR_POWERUP_PTR      0x27

/* PCIe control */
#define I210_NVM_PCIE_CTRL_W2         0x28
#define I210_NVM_PCIE_CTRL_W3         0x29

/* Read-only area */
#define I210_NVM_END_OF_RO_AREA       0x2C
#define I210_NVM_START_OF_RO_AREA     0x2D

/* Watchdog / VPD */
#define I210_NVM_WATCHDOG_CONFIG      0x2E
#define I210_NVM_VPD_PTR              0x2F

/* PXE */
#define I210_NVM_PXE_SETUP_OPTIONS    0x30
#define I210_NVM_PXE_CONFIG_OPTIONS   0x31
#define I210_NVM_PXE_VERSION          0x32
#define I210_NVM_PXE_ROM_CAPABILITIES 0x33

#define I210_NVM_PXE_ISCSI_ROM_VER    0x36
#define I210_NVM_PXE_ALT_MAC_PTR      0x37

#define I210_NVM_PXE_ISCSI_BOOT_PTR   0x3D

/* NVM checksum */
#define I210_NVM_CHECKSUM_WORD        0x3F

/* Free provisioning area */
#define I210_NVM_FREE_PROV_AREA_PTR   0x40
#define I210_NVM_FREE_PROV_AREA_SIZE  0x41

/* Image unique ID */
#define I210_NVM_IMAGE_UNIQUE_ID_W0   0x42
#define I210_NVM_IMAGE_UNIQUE_ID_W1   0x43

/* Firmware */
#define I210_NVM_FW_RO_UPDATE_VER     0x50
#define I210_NVM_FW_MODULE_PTR        0x51

/* Device Control Register */
#define I210_CTRL              0x00000
#define I210_CTRL_RST          BIT(26)

/* Device Status Register */
#define I210_STATUS            0x00008
#define I210_STATUS_PF_RST_DONE BIT(21)

/* EEPROM/NVM Control Register */
#define I210_EEC               0x12010
#define I210_EEC_AUTO_RD       BIT(9)

/* Receive Address Low (RAL) and High (RAH)*/
#define I210_RAL0       0x05400
#define I210_RAH0       0x05404
#define I210_RAH_AV     BIT(31)

/* Transmit Control Register */
#define I210_TCTL           0x00400
#define I210_TCTL_EN        BIT(1)
#define I210_TCTL_PSP       BIT(3)
#define I210_TCTL_RTLC      BIT(24)
#define I210_TCTL_CT_MASK   GENMASK(11, 4)

/* Receive Control Register */
#define I210_RCTL          0x00100
#define I210_RCTL_EN       BIT(1)
#define I210_RCTL_UPE      BIT(3)
#define I210_RCTL_MPE      BIT(4)
#define I210_RCTL_BAM      BIT(15)
#define I210_RCTL_SECRC    BIT(26)

/* TX descriptor ring - queue 0 */
#define I210_TDBAL(q)       (0x03800 + ((q) * 0x100))
#define I210_TDBAH(q)       (0x03804 + ((q) * 0x100))
#define I210_TDLEN(q)       (0x03808 + ((q) * 0x100))
#define I210_TDH(q)         (0x03810 + ((q) * 0x100))
#define I210_TDT(q)         (0x03818 + ((q) * 0x100))
#define I210_TXDCTL(q)      (0x03828 + ((q) * 0x100))

#endif /* _I210_DEFINES_H_ */