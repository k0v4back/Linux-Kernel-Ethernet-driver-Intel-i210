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