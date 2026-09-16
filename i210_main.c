#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>

#include "i210_defines.h"
#include "i210_helpers.h"
#include "i210.h"


static const struct pci_device_id i210_pci_tbl[] = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x1533),
	},
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, i210_pci_tbl);


static struct pci_driver i210_driver = {
	.name		= DRV_NAME,
	.id_table	= i210_pci_tbl,
	.probe		= i210_probe,
	.remove		= i210_remove,
	.shutdown	= i210_shutdown,
};


static int i210_open(struct net_device *netdev)
{
	struct i210_adapter *adapter = netdev_priv(netdev);
	int err;

	pr_info(DRV_NAME ": i210_open()\n");

	err = i210_reset_hw(adapter);
	if (err) {
		pr_err(DRV_NAME ": hardware reset failed: %d\n", err);
		return err;
	}

	/*
	 * Set MAC address
	 * TX configuration
	 * RX configuration
	 */
	i210_set_mac_address(adapter, netdev->dev_addr);
	i210_configure_tx(adapter);
	i210_configure_rx(adapter);

	pr_info(DRV_NAME ": hardware initialization completed\n");
	// netif_start_queue(netdev);

	return 0;
}

static int i210_close(struct net_device *netdev)
{
	struct i210_adapter *adapter = netdev_priv(netdev);

	pr_info(DRV_NAME ": i210_close()\n");

	netif_stop_queue(netdev);

	return 0;
}

static netdev_tx_t i210_xmit(struct sk_buff *skb,
			     struct net_device *netdev)
{
	struct i210_adapter *adapter = netdev_priv(netdev);

	pr_info(DRV_NAME ": i210_xmit()\n");

	dev_kfree_skb(skb);

	return NETDEV_TX_OK;
}


static void i210_get_drvinfo(struct net_device *netdev,
			     struct ethtool_drvinfo *drvinfo)
{
	struct i210_adapter *adapter = netdev_priv(netdev);

	strlcpy(drvinfo->driver, DRV_NAME, sizeof(drvinfo->driver));
	strlcpy(drvinfo->bus_info, pci_name(adapter->pdev), sizeof(drvinfo->bus_info));

	pr_info(DRV_NAME ": i210_get_drvinfo()\n");
}

static const struct ethtool_ops i210_ethtool_ops = {
	.supported_coalesce_params = ETHTOOL_COALESCE_RX_USECS,
	.get_drvinfo = i210_get_drvinfo,
};



static int i210_clean(struct napi_struct *napi, int budget)
{
	int work_done = 0;

	/*
	 * RX/TX processing will be implemented later.
	 */

	return work_done;
}



static const struct net_device_ops i210_netdev_ops = {
	.ndo_open			= i210_open,
	.ndo_stop			= i210_close,
	.ndo_start_xmit		= i210_xmit,
};



static int i210_probe(struct pci_dev *pdev,
		      const struct pci_device_id *ent)
{
	struct net_device *netdev;
	struct i210_adapter *adapter;
	int bars;
	int err;
	u16 vendor_id;
	u16 device_id;
	u16 nvm_word[3];
	u8 mac[6];


	pr_info(DRV_NAME ": probe()\n");
	pr_info(DRV_NAME ": PCI device %04x:%04x\n",
		pdev->vendor, pdev->device);

	/* 1. Determine MMIO BARs */
	bars = pci_select_bars(pdev, IORESOURCE_MEM);
	if (!bars) {
		pr_err(DRV_NAME ": no MMIO BARs found\n");
		return -ENODEV;
	}
	pr_info(DRV_NAME ": memory BARs = 0x%x\n", bars);

	/* 2. Enable PCI device */
	err = pci_enable_device_mem(pdev);
	if (err)
		return err;

	/* 3. Request BARs */
	err = pci_request_selected_regions(pdev, bars, DRV_NAME);
	if (err)
		goto err_disable_device;

	/* 4. Enable bus mastering */
	pci_set_master(pdev);

	/* 5. Allocate net_device + private data */
	netdev = alloc_etherdev(sizeof(struct i210_adapter));
	if (!netdev) {
		err = -ENOMEM;
		goto err_release_regions;
	}

	/* 6. Get private data */
	adapter = netdev_priv(netdev);

	adapter->pdev = pdev;
	adapter->netdev = netdev;

	/* 7. Map BAR0 */
	adapter->hw.hw_addr = pci_iomap(pdev, 0, 0);
	if (!adapter->hw.hw_addr) {
		pr_err(DRV_NAME ": failed to map BAR0\n");
		err = -EIO;
		goto err_free_netdev;
	}

	/* 8. Save driver data */
	pci_set_drvdata(pdev, netdev);

	/* 9. Read MMIO register */
	pci_read_config_word(pdev, PCI_VENDOR_ID, &vendor_id);
	pci_read_config_word(pdev, PCI_DEVICE_ID, &device_id);
	pr_info(DRV_NAME ": PCI Vendor ID = 0x%04x\n", vendor_id);
	pr_info(DRV_NAME ": PCI Device ID = 0x%04x\n", device_id);
	int i;
	for (i = 0; i < 3; i++) {
		err = i210_read_nvm_word(adapter, i, &nvm_word[i]);
		if (err)
			return err;

		pr_info(DRV_NAME ": NVM word[0x%02x] = 0x%04x\n", i, nvm_word[i]);
	}
	mac[0] = nvm_word[0] & 0xff;
	mac[1] = nvm_word[0] >> 8;
	mac[2] = nvm_word[1] & 0xff;
	mac[3] = nvm_word[1] >> 8;
	mac[4] = nvm_word[2] & 0xff;
	mac[5] = nvm_word[2] >> 8;
	eth_hw_addr_set(netdev, mac);
	pr_info(DRV_NAME ": MAC address %pM\n", netdev->dev_addr);


	/* 10. Configure net_device */
	netdev->netdev_ops = &i210_netdev_ops;
	netdev->ethtool_ops = &i210_ethtool_ops;
	netdev->watchdog_timeo = 5 * HZ;
	netif_napi_add(netdev, &adapter->napi, i210_clean, 64);
	strcpy(netdev->name, "eth%d");

	/* 11. Register net_device */
	err = register_netdev(netdev);
	if (err) {
		pr_err(DRV_NAME ": register_netdev() failed: %d\n", err);
		goto err_napi;
	}

	pr_info(DRV_NAME ": network device registered as %s\n",
		netdev->name);

	return 0;


err_napi:
	netif_napi_del(&adapter->napi);
	pci_iounmap(pdev, adapter->hw.hw_addr);
	pci_set_drvdata(pdev, NULL);

err_free_netdev:
	free_netdev(netdev);

err_release_regions:
	pci_release_selected_regions(pdev, bars);

err_disable_device:
	pci_disable_device(pdev);

	return err;
}



static void i210_remove(struct pci_dev *pdev)
{
	struct net_device *netdev;
	struct i210_adapter *adapter;

	pr_info(DRV_NAME ": remove()\n");

	netdev = pci_get_drvdata(pdev);

	if (!netdev)
		return;

	adapter = netdev_priv(netdev);

	unregister_netdev(netdev);

	netif_napi_del(&adapter->napi);

	if (adapter->hw.hw_addr)
		pci_iounmap(pdev, adapter->hw.hw_addr);

	pci_release_selected_regions(pdev,
				     pci_select_bars(pdev, IORESOURCE_MEM));

	pci_clear_master(pdev);
	pci_disable_device(pdev);

	pci_set_drvdata(pdev, NULL);

	free_netdev(netdev);
}


static void i210_shutdown(struct pci_dev *pdev)
{
	pr_info(DRV_NAME ": shutdown()\n");
}



static int __init i210_init(void)
{
	pr_info(DRV_NAME ": init()\n");

	return pci_register_driver(&i210_driver);
}

static void __exit i210_exit(void)
{
	pr_info(DRV_NAME ": exit()\n");

	pci_unregister_driver(&i210_driver);
}


module_init(i210_init);
module_exit(i210_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vadim Kosolapov");
MODULE_DESCRIPTION("Intel I210 Ethernet driver");