#ifndef _I210_DEFINES_H_
#define _I210_DEFINES_H_

#define I210_EERD          0x12014

#define EERD_START         BIT(0)
#define EERD_DONE          BIT(1)
#define EERD_ADDR_SHIFT    2
#define EERD_DATA_SHIFT    16

#define DRV_NAME "i210_driver"

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

#endif /* _I210_DEFINES_H_ */