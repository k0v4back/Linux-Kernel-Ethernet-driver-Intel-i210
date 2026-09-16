#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>

#include "i210_helpers.h"

static int i210_read_nvm_word(struct i210_adapter *adapter, u16 offset, u16 *data)
{
	u32 value;
	int timeout = 1000;

	/* Start NVM read */
	value = EERD_START | ((u32)offset << EERD_ADDR_SHIFT);

	writel(value, adapter->hw.hw_addr + I210_EERD);

	do {
		value = readl(adapter->hw.hw_addr + I210_EERD);

		if (value & EERD_DONE) {
			*data = (u16)(value >> EERD_DATA_SHIFT);
			return 0;
		}

		udelay(1);
	} while (--timeout);

	pr_err(DRV_NAME ": NVM read timeout, offset=0x%04x\n", offset);

	return -ETIMEDOUT;
}


/*
 * 4.3.1 Software Reset (RST)
 */
static int i210_reset_hw(struct i210_adapter *adapter)
{
	void __iomem *hw = adapter->hw.hw_addr;
	u32 ctrl;
	u32 status;
	u32 eec;

	pr_info(DRV_NAME ": hardware reset started\n");

	/*
	 * Read current CTRL value.
	 */
	ctrl = readl(hw + I210_CTRL);
	pr_info(DRV_NAME ": CTRL before reset = 0x%08x\n", ctrl);

	/*
	 * Assert software reset.
	 */
	ctrl |= I210_CTRL_RST;
	writel(ctrl, hw + I210_CTRL);

	msleep(3);

	/*
	 * Verify that software reset has completed.
	 */
	status = readl(hw + I210_STATUS);
	pr_info(DRV_NAME ": STATUS after reset = 0x%08x\n", status);

	if (!(status & I210_STATUS_PF_RST_DONE)) {
		pr_err(DRV_NAME ": reset did not complete\n");
		return -ETIMEDOUT;
	}

	/*
	 * Verify NVM auto-read completed.
	 */
	eec = readl(hw + I210_EEC);
	pr_info(DRV_NAME ": EEC after reset = 0x%08x\n", eec);

	if (!(eec & I210_EEC_AUTO_RD)) {
		pr_err(DRV_NAME ": NVM auto-read is not complete\n");
		return -EIO;
	}

	pr_info(DRV_NAME ": hardware reset completed successfully\n");

	return 0;
}


/*
 * 6.2.1 Ethernet Address (Words 0x00-0x02)
 * 8.10.16 Receive Address Low - RAL
 * 8.10.17 Receive Address High - RAH
 * 10.6.4.4.2.1 MAC Address Filtering
 *
 * Write MAC to RAL and RAH for hardware MAC filtering
 *
 * MAC:
 *   mac[0] mac[1] mac[2] mac[3] mac[4] mac[5]
 * RAL:
 *   mac[0] mac[1] mac[2] mac[3]
 * RAH:
 *   mac[4] mac[5]
 */
static void i210_set_mac_address(struct i210_adapter *adapter, const u8 *mac)
{
	void __iomem *hw = adapter->hw.hw_addr;
	u32 ral, rah;

	ral = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
	rah = mac[4] | (mac[5] << 8);

	writel(ral, hw + I210_RAL0);

	/*
	 * Address is valid only when AV is set
	 */
	writel(rah | I210_RAH_AV, hw + I210_RAH0);

	pr_info(DRV_NAME ": RAL0 = 0x%08x\n", ral);
	pr_info(DRV_NAME ": RAH0 = 0x%08x\n", rah | I210_RAH_AV);	
}


static void i210_configure_tx(struct i210_adapter *adapter)
{
	void __iomem *hw = adapter->hw.hw_addr;
	u32 tctl;

	/* It's some about half-duplex collision behavior */
	tctl = readl(hw + I210_TCTL);
	tctl &= ~I210_TCTL_CT_MASK;

	/*
	 * Pad Short Packets(PSP)
	 * Retransmit on late collision(RTLC)
	 */
	tctl |= I210_TCTL_PSP;
	tctl |= I210_TCTL_RTLC;

	/*
	 * Do NOT enable TX yet.
	 * TX ring is not initialized(will enable in future)
	 */
	tctl &= ~I210_TCTL_EN;

	writel(tctl, hw + I210_TCTL);
	pr_info(DRV_NAME ": TCTL = 0x%08x\n", tctl);
}


static void i210_configure_rx(struct i210_adapter *adapter)
{
	void __iomem *hw = adapter->hw.hw_addr;
	u32 rctl;

	rctl = readl(hw + I210_RCTL);

	/*
	 * Basic receive filtering
	 *
	 * Receive engine remains disabled until RX ring
	 * and DMA buffers are initialized(will enable in future)
	 */
	rctl &= ~I210_RCTL_EN;

	/*
	 * Broadcast Accept Mode(BAM)
	 * Strip Ethernet CRC From Incoming Packet(SECRC)
	 */
	rctl |= I210_RCTL_BAM;
	rctl |= I210_RCTL_SECRC;

	writel(rctl, hw + I210_RCTL);
	pr_info(DRV_NAME ": RCTL = 0x%08x\n", rctl);
}


static int i210_setup_tx_resources(struct i210_adapter *adapter)
{
	
}