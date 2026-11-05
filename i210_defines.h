
#ifndef _I210_DEFINES_H_
#define _I210_DEFINES_H_

#include <linux/bitops.h>

/* -------------------------------------------------------------------------
 * General driver definitions
 * ------------------------------------------------------------------------- */

#define DRV_NAME "i210"

#define I210_TX_DESC_COUNT 64
#define I210_RX_DESC_COUNT 64

/*
 * Size of one RX packet buffer allocated in host memory.
 * This is not the size of the NIC internal RX packet buffer.
 */
#define I210_RX_BUFFER_SIZE 2048

/* -------------------------------------------------------------------------
 * NVM / EEPROM registers
 * ------------------------------------------------------------------------- */

/*
 * EEPROM Read (EERD)
 *
 * Used to read 16-bit words from the device NVM.
 * The driver starts a read operation and polls the DONE bit.
 */
#define I210_EERD 0x12014

#define EERD_START BIT(0)
#define EERD_DONE BIT(1)
#define EERD_ADDR_SHIFT 2
#define EERD_DATA_SHIFT 16

/* EEPROM/NVM Control Register */
#define I210_EEC 0x12010
#define I210_EEC_AUTO_RD BIT(9)

/* -------------------------------------------------------------------------
 * Device control and status registers
 * ------------------------------------------------------------------------- */

/* Device Control Register */
#define I210_CTRL 0x00000
#define I210_CTRL_SLU BIT(6)
#define I210_CTRL_RST BIT(26)

/* Device Status Register */
#define I210_STATUS 0x00008
#define I210_STATUS_PF_RST_DONE BIT(21)

/* -------------------------------------------------------------------------
 * MAC address registers
 * ------------------------------------------------------------------------- */

/*
 * Receive Address registers
 *
 * RAL0 stores the first four bytes of the MAC address.
 * RAH0 stores the remaining two bytes.
 */
#define I210_RAL0 0x05400
#define I210_RAH0 0x05404

/* Address Valid bit in RAH */
#define I210_RAH_AV BIT(31)

/* -------------------------------------------------------------------------
 * MAC transmit and receive control registers
 * ------------------------------------------------------------------------- */

/* Transmit Control Register */
#define I210_TCTL 0x00400

#define I210_TCTL_EN BIT(1)
#define I210_TCTL_PSP BIT(3)
#define I210_TCTL_RTLC BIT(24)

#define I210_TCTL_CT_SHIFT 4
#define I210_TCTL_CT_MASK GENMASK(11, 4)
#define I210_TCTL_CT (15U << I210_TCTL_CT_SHIFT)

/* Receive Control Register */
#define I210_RCTL 0x00100

#define I210_RCTL_EN BIT(1)
#define I210_RCTL_UPE BIT(3)
#define I210_RCTL_MPE BIT(4)
#define I210_RCTL_BAM BIT(15)
#define I210_RCTL_SECRC BIT(26)

/* Additional MAC configuration registers */
#define I210_TIPG 0x00410
#define I210_TCTL_EXT 0x00404
#define I210_DTXCTL 0x03590

/* -------------------------------------------------------------------------
 * TX descriptor ring registers - queue 0
 * ------------------------------------------------------------------------- */

/*
 * TX descriptor ring registers
 *
 * The NIC reads TX descriptors from host memory using DMA.
 *
 * TDBAL/TDBAH - base DMA address of the descriptor ring
 * TDLEN       - descriptor ring size in bytes
 * TDH         - hardware head index
 * TDT         - software tail index
 */
#define I210_TDBAL(q) (0x03800 + ((q) * 0x100))
#define I210_TDBAH(q) (0x03804 + ((q) * 0x100))
#define I210_TDLEN(q) (0x03808 + ((q) * 0x100))
#define I210_TDH(q) (0x03810 + ((q) * 0x100))
#define I210_TDT(q) (0x03818 + ((q) * 0x100))

/*
 * TX Descriptor Control Register
 *
 * PTHRESH - descriptor prefetch threshold
 * HTHRESH - descriptor host threshold
 * WTHRESH - descriptor write-back threshold
 */
#define I210_TXDCTL(q) (0x03828 + ((q) * 0x100))

#define I210_TXDCTL_PTHRESH_SHIFT 0
#define I210_TXDCTL_HTHRESH_SHIFT 8
#define I210_TXDCTL_WTHRESH_SHIFT 16

#define I210_TXDCTL_WTHRESH (1U << I210_TXDCTL_WTHRESH_SHIFT)
#define I210_TXDCTL_ENABLE BIT(25)

/*
 * Advanced TX descriptor fields
 *
 * DTYP_ADV - advanced descriptor type
 * DEXT     - extended descriptor format
 * EOP      - end of packet
 * IFCS     - insert Ethernet frame check sequence
 * RS       - request status write-back
 */
#define I210_TXD_PAYLEN_SHIFT 14

#define I210_TXD_CMD_EOP BIT(0)
#define I210_TXD_CMD_IFCS BIT(1)
#define I210_TXD_CMD_RS BIT(3)
#define I210_TXD_CMD_DEXT BIT(5)

#define I210_TXD_DTYP_SHIFT 20
#define I210_TXD_CMD_SHIFT 24
#define I210_TXD_DTYP_ADV 0x3

/* TX descriptor status */
#define I210_TXD_STAT_DD BIT(0)

/* -------------------------------------------------------------------------
 * RX descriptor ring registers - queue 0
 * ------------------------------------------------------------------------- */

/*
 * RX descriptor ring registers
 *
 * The NIC writes received packet data into host memory using DMA.
 *
 * RDBAL/RDBAH - base DMA address of the descriptor ring
 * RDLEN       - descriptor ring size in bytes
 * RDH         - hardware head index
 * RDT         - software tail index
 * SRRCTL      - RX descriptor and packet buffer configuration
 */
#define I210_RDBAL(q) (0x0C000 + ((q) * 0x100))
#define I210_RDBAH(q) (0x0C004 + ((q) * 0x100))
#define I210_RDLEN(q) (0x0C008 + ((q) * 0x100))
#define I210_SRRCTL(q) (0x0C00C + ((q) * 0x100))
#define I210_RDH(q) (0x0C010 + ((q) * 0x100))
#define I210_RDT(q) (0x0C018 + ((q) * 0x100))

/* RX Descriptor Control Register */
#define I210_RXDCTL(q) (0x0C028 + ((q) * 0x100))
#define I210_RXDCTL_ENABLE BIT(25)

/*
 * RX descriptor status bits
 *
 * DD  - descriptor done
 * EOP - end of packet
 */
#define I210_RXD_STAT_DD BIT(0)
#define I210_RXD_STAT_EOP BIT(1)

/* -------------------------------------------------------------------------
 * MAC / PHY Management Interface - MDIC
 * ------------------------------------------------------------------------- */

/*
 * MDIC is used by the MAC to access PHY registers.
 *
 * PHY register address is placed in bits 20:16.
 * Operation code is placed in bits 27:26.
 * READY is set by hardware when the transaction completes.
 * ERROR indicates a failed PHY transaction.
 */
#define I210_MDIC 0x00020

#define I210_MDIC_DATA_MASK 0x0000ffff
#define I210_MDIC_REG_SHIFT 16
#define I210_MDIC_OP_SHIFT 26

#define I210_MDIC_OP_READ (2U << I210_MDIC_OP_SHIFT)
#define I210_MDIC_READY BIT(28)
#define I210_MDIC_ERROR BIT(30)

/* -------------------------------------------------------------------------
 * Standard IEEE 802.3 Clause 22 PHY registers
 * ------------------------------------------------------------------------- */

/*
 * These are PHY register numbers, not MMIO offsets.
 * The registers are accessed through the MAC MDIC interface.
 */
#define I210_PHY_CTRL 0
#define I210_PHY_STATUS 1
#define I210_PHY_ID1 2
#define I210_PHY_ID2 3
#define I210_PHY_AN_ADV 4
#define I210_PHY_AN_LP_ABILITY 5
#define I210_PHY_AN_EXP 6
#define I210_PHY_1000_CTRL 9
#define I210_PHY_1000_STATUS 10

/*
 * Basic Control Register (BMCR, register 0)
 */
#define I210_PHY_CTRL_RESET BIT(15)
#define I210_PHY_CTRL_LOOPBACK BIT(14)
#define I210_PHY_CTRL_SPEED_LSB BIT(13)
#define I210_PHY_CTRL_AN_ENABLE BIT(12)
#define I210_PHY_CTRL_POWER_DOWN BIT(11)
#define I210_PHY_CTRL_RESTART_AN BIT(9)
#define I210_PHY_CTRL_DUPLEX BIT(8)
#define I210_PHY_CTRL_SPEED_MSB BIT(6)

/*
 * Basic Status Register (BMSR, register 1)
 */
#define I210_PHY_STATUS_100BASE_T4 BIT(15)
#define I210_PHY_STATUS_100BASE_XFD BIT(14)
#define I210_PHY_STATUS_100BASE_XHD BIT(13)
#define I210_PHY_STATUS_10FD BIT(12)
#define I210_PHY_STATUS_10HD BIT(11)
#define I210_PHY_STATUS_100BASE_T2FD BIT(10)
#define I210_PHY_STATUS_100BASE_T2HD BIT(9)
#define I210_PHY_STATUS_EXT_STATUS BIT(8)
#define I210_PHY_STATUS_MF_PREAMBLE BIT(6)
#define I210_PHY_STATUS_AN_COMPLETE BIT(5)
#define I210_PHY_STATUS_REMOTE_FAULT BIT(4)
#define I210_PHY_STATUS_AN_ABILITY BIT(3)
#define I210_PHY_STATUS_LINK_STATUS BIT(2)
#define I210_PHY_STATUS_JABBER BIT(1)
#define I210_PHY_STATUS_EXT_CAP BIT(0)

/*
 * 1000BASE-T Control Register (register 9)
 */
#define I210_PHY_1000_CTRL_MS_MANUAL BIT(12)
#define I210_PHY_1000_CTRL_MS_VALUE BIT(11)
#define I210_PHY_1000_CTRL_TEST_SHIFT 13
#define I210_PHY_1000_CTRL_TEST_MASK GENMASK(15, 13)
#define I210_PHY_1000_CTRL_1000FD BIT(9)
#define I210_PHY_1000_CTRL_1000HD BIT(8)

/*
 * PHY BMCR aliases used by PHY power-up code.
 */
#define I210_PHY_BMCR 0
#define I210_PHY_BMCR_POWER_DOWN BIT(11)
#define I210_PHY_BMCR_ISOLATE BIT(10)
#define I210_PHY_BMCR_AN_ENABLE BIT(12)
#define I210_PHY_BMCR_RESTART_AN BIT(9)

/* -------------------------------------------------------------------------
 * Internal TX packet buffer configuration
 * ------------------------------------------------------------------------- */

/*
 * TX Packet Buffer Size Register
 *
 * Configures the internal packet buffer allocation inside the NIC.
 * This is separate from the TX descriptor ring and packet buffers
 * allocated in host memory by the driver.
 */
#define I210_TXPBS 0x03404

/*
 * Default TX packet buffer configuration:
 * TXPB0 = 20 KB
 * OS2BMC packet buffer = 4 KB
 */
#define I210_TXPBSIZE_DEFAULT 0x04000014

/*
 * Maximum Transmit Packet Size Register
 *
 * The value is specified in 64-byte units.
 */
#define I210_DTXMXPKTSZ 0x0355C
#define I210_DTXMXPKTSZ_DEFAULT 0x00000098

/* -------------------------------------------------------------------------
 * Statistics registers
 * ------------------------------------------------------------------------- */

/*
 * Receive statistics
 */
#define I210_GPRC 0x04074
#define I210_BPRC 0x04078
#define I210_MPRC 0x0407C
#define I210_RNBC 0x040A0

/*
 * Transmit statistics
 */
#define I210_TPR 0x040D0

/* -------------------------------------------------------------------------
 * Interrupt registers
 * ------------------------------------------------------------------------- */

/*
 * Legacy interrupt registers:
 * ICR - Interrupt Cause Read
 * ICS - Interrupt Cause Set
 * IMS - Interrupt Mask Set
 * IMC - Interrupt Mask Clear
 * IAM - Interrupt Acknowledge Auto Mask
 */
#define I210_ICR 0x000C0
#define I210_ICS 0x000C8
#define I210_IMS 0x000D0
#define I210_IMC 0x000D8
#define I210_IAM 0x000E0

/*
 * Extended interrupt registers:
 * EICS - Extended Interrupt Cause Set
 * EIMS - Extended Interrupt Mask Set
 * EIMC - Extended Interrupt Mask Clear
 * EIAC - Extended Interrupt Auto Clear
 * EIAM - Extended Interrupt Auto Mask
 * EICR - Extended Interrupt Cause Read
 */
#define I210_EICS 0x01520
#define I210_EIMS 0x01524
#define I210_EIMC 0x01528
#define I210_EIAC 0x0152C
#define I210_EIAM 0x01530
#define I210_EICR 0x01580

/*
 * Interrupt Vector Allocation Register
 */
#define I210_IVAR 0x01700

#define I210_EICR_RXTXQ0 BIT(0)

#define I210_IVAR_RX0_SHIFT 0
#define I210_IVAR_RX0_VALID BIT(7)

#endif /* _I210_DEFINES_H_ */
