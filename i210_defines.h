#ifndef _I210_DEFINES_H_
#define _I210_DEFINES_H_

#define I210_EERD          0x12014

#define EERD_START         BIT(0)
#define EERD_DONE          BIT(1)
#define EERD_ADDR_SHIFT    2
#define EERD_DATA_SHIFT    16

#define I210_TX_DESC_COUNT    64

#define I210_RX_DESC_COUNT 64
#define I210_RX_BUFFER_SIZE 2048


#define DRV_NAME "i210"


/* Device Control Register */
#define I210_CTRL              0x00000
#define I210_CTRL_SLU          BIT(6)
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
#define I210_TXDCTL_PTHRESH_SHIFT 0
#define I210_TXDCTL_HTHRESH_SHIFT 8
#define I210_TXDCTL_WTHRESH_SHIFT 16
#define I210_TXDCTL_WTHRESH        (1U << I210_TXDCTL_WTHRESH_SHIFT)
#define I210_TXDCTL_ENABLE BIT(25)
#define I210_TXD_PAYLEN_SHIFT 14
#define I210_TXD_CMD_EOP   BIT(0)
#define I210_TXD_CMD_IFCS  BIT(1)
#define I210_TXD_CMD_RS    BIT(3)
#define I210_TXD_CMD_DEXT  BIT(5)
#define I210_TXD_DTYP_SHIFT 20
#define I210_TXD_CMD_SHIFT 24
#define I210_TXD_DTYP_ADV   0x3

#define I210_TXD_STAT_DD    BIT(0)

/* RX descriptor ring - queue 0 */
#define I210_RDBAL(q)   (0x0C000 + ((q) * 0x100))
#define I210_RDBAH(q)   (0x0C004 + ((q) * 0x100))
#define I210_RDLEN(q)   (0x0C008 + ((q) * 0x100))
#define I210_SRRCTL(q)  (0x0C00C + ((q) * 0x100))
#define I210_RDH(q)     (0x0C010 + ((q) * 0x100))
#define I210_RDT(q)     (0x0C018 + ((q) * 0x100))
#define I210_RXDCTL(q)  (0x0C028 + ((q) * 0x100))
#define I210_RXDCTL_ENABLE BIT(25)
#define I210_RXD_CMD_EOP   BIT(0)
#define I210_RXD_CMD_IFCS  BIT(1)
#define I210_RXD_CMD_RS    BIT(3)
#define I210_RXD_CMD_DEXT  BIT(5)

#define I210_RXD_STAT_DD BIT(0)
#define I210_RXD_STAT_EOP BIT(1)

/* MDIC registers */
#define I210_MDIC              0x00020
#define I210_MDIC_DATA_MASK   0x0000ffff
#define I210_MDIC_REG_SHIFT   16
#define I210_MDIC_OP_SHIFT    26
#define I210_MDIC_OP_READ     (2U << I210_MDIC_OP_SHIFT)
#define I210_MDIC_READY       BIT(28)
#define I210_MDIC_ERROR       BIT(30)

/*
 * PHY registers
 *
 * I210 internal PHY is accessed through MDIC.
 */
#define I210_PHY_CTRL             0
#define I210_PHY_STATUS           1
#define I210_PHY_ID1              2
#define I210_PHY_ID2              3
#define I210_PHY_AN_ADV           4
#define I210_PHY_AN_LP_ABILITY    5
#define I210_PHY_AN_EXP           6
#define I210_PHY_1000_CTRL         9
#define I210_PHY_1000_STATUS      10

/*
 * PHY Copper Control Register (reg 0)
 */
#define I210_PHY_CTRL_RESET       BIT(15)
#define I210_PHY_CTRL_LOOPBACK    BIT(14)
#define I210_PHY_CTRL_SPEED_LSB   BIT(13)
#define I210_PHY_CTRL_AN_ENABLE   BIT(12)
#define I210_PHY_CTRL_POWER_DOWN  BIT(11)
#define I210_PHY_CTRL_RESTART_AN  BIT(9)
#define I210_PHY_CTRL_DUPLEX      BIT(8)
#define I210_PHY_CTRL_SPEED_MSB   BIT(6)

/*
 * PHY Copper Status Register (reg 1)
 */
#define I210_PHY_STATUS_100BASE_T4    BIT(15)
#define I210_PHY_STATUS_100BASE_XFD   BIT(14)
#define I210_PHY_STATUS_100BASE_XHD   BIT(13)
#define I210_PHY_STATUS_10FD           BIT(12)
#define I210_PHY_STATUS_10HD           BIT(11)
#define I210_PHY_STATUS_100BASE_T2FD  BIT(10)
#define I210_PHY_STATUS_100BASE_T2HD  BIT(9)
#define I210_PHY_STATUS_EXT_STATUS     BIT(8)
#define I210_PHY_STATUS_MF_PREAMBLE    BIT(6)
#define I210_PHY_STATUS_AN_COMPLETE    BIT(5)
#define I210_PHY_STATUS_REMOTE_FAULT   BIT(4)
#define I210_PHY_STATUS_AN_ABILITY     BIT(3)
#define I210_PHY_STATUS_LINK_STATUS    BIT(2)
#define I210_PHY_STATUS_JABBER         BIT(1)
#define I210_PHY_STATUS_EXT_CAP        BIT(0)

/*
 * 1000BASE-T Control Register (reg 9)
 */
#define I210_PHY_1000_CTRL_MS_MANUAL   BIT(12)
#define I210_PHY_1000_CTRL_MS_VALUE    BIT(11)
#define I210_PHY_1000_CTRL_TEST_SHIFT  13
#define I210_PHY_1000_CTRL_TEST_MASK   GENMASK(15, 13)
#define I210_PHY_1000_CTRL_1000FD      BIT(9)
#define I210_PHY_1000_CTRL_1000HD      BIT(8)

#define I210_PHY_BMCR             0
#define I210_PHY_BMCR_POWER_DOWN  BIT(11)
#define I210_PHY_BMCR_ISOLATE     BIT(10)
#define I210_PHY_BMCR_AN_ENABLE   BIT(12)
#define I210_PHY_BMCR_RESTART_AN  BIT(9)

#define I210_TXPBS               0x03404
#define I210_TXPBSIZE_DEFAULT    0x04000014

#define I210_DTXMXPKTSZ          0x0355C
#define I210_DTXMXPKTSZ_DEFAULT  0x00000098

#define I210_TIPG       0x00410
#define I210_TCTL_EXT   0x00404
#define I210_DTXCTL     0x03590
#define I210_DTXMXPKTSZ 0x0355C

#define I210_GPRC   0x04074
#define I210_BPRC   0x04078
#define I210_MPRC   0x0407C
#define I210_TPR    0x040D0
#define I210_RNBC   0x040A0

#define I210_TCTL_CT_SHIFT 4
#define I210_TCTL_CT       (15U << I210_TCTL_CT_SHIFT)


#define I210_ICR              0x000C0
#define I210_ICS              0x000C8
#define I210_IMS              0x000D0
#define I210_IMC              0x000D8
#define I210_IAM              0x000E0
#define I210_EICS             0x01520
#define I210_EIMS             0x01524
#define I210_EIMC             0x01528
#define I210_EIAC             0x0152C
#define I210_EIAM             0x01530
#define I210_EICR             0x01580

#define I210_IVAR             0x01700

#define I210_EICR_RXTXQ0     BIT(0)
#define I210_IVAR_RX0_SHIFT  0
#define I210_IVAR_RX0_VALID  BIT(7)


#endif /* _I210_DEFINES_H_ */