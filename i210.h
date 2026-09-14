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
	void __iomem *hw_addr;
	void __iomem *flash_address;
};

struct i210_adapter {
	struct pci_dev *pdev;
	struct net_device *netdev;
	struct i210_hw hw;

	struct napi_struct napi;
};

static int i210_probe(struct pci_dev *pdev,
		      const struct pci_device_id *ent);
static void i210_remove(struct pci_dev *pdev);
static void i210_shutdown(struct pci_dev *pdev);

static int i210_open(struct net_device *netdev);
static int i210_close(struct net_device *netdev);
static netdev_tx_t i210_xmit(struct sk_buff *skb,
			     struct net_device *netdev);

static int i210_clean(struct napi_struct *napi, int budget);

static void i210_get_drvinfo(struct net_device *netdev,
			     struct ethtool_drvinfo *drvinfo);

#endif /* _I210_H_ */