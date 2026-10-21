#ifndef _I210_H_
#define _I210_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>

struct i210_hw {
	void __iomem *hw_addr; // Address of MMIO area in the virtual address space
	void __iomem *flash_address;
};

struct i210_adapter {
	struct pci_dev *pdev;
	struct net_device *netdev;
	struct i210_hw hw;

	struct napi_struct napi;
};

#endif /* _I210_H_ */