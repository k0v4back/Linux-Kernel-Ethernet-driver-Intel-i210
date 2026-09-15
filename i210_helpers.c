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