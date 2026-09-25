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

	/*
	 * Hardware reset
	 */
	err = i210_reset_hw(adapter);
	if (err) {
		pr_err(DRV_NAME ": hardware reset failed: %d\n", err);
		return err;
	}

	/*
	 * Restore/program MAC address
	 */
	i210_set_mac_address(adapter, netdev->dev_addr);

	/*
	 * Initialize internal PHY and start auto-negotiation
	 */
	err = i210_init_phy(adapter);
	if (err) {
		pr_err(DRV_NAME ": PHY init failed: %d\n", err);
		return err;
	}

	/*
	 * Setup TX resources
	 */
	err = i210_setup_tx_resources(adapter);
	if (err) {
		pr_err(DRV_NAME ": TX resource setup failed: %d\n", err);
		return err;
	}

	/*
	 * Setup RX resources
	 */
	err = i210_setup_rx_resources(adapter);
	if (err) {
		pr_err(DRV_NAME ": RX resource setup failed: %d\n", err);
		goto err_free_tx;
	}

	/*
	 * Configure TX ring
	 */
	err = i210_configure_tx_ring(adapter);
	if (err) {
		pr_err(DRV_NAME ": TX ring configuration failed: %d\n", err);
		goto err_free_rx;
	}

	/*
	 * Configure RX ring
	 */
	err = i210_configure_rx_ring(adapter);
	if (err) {
		pr_err(DRV_NAME ": RX ring configuration failed: %d\n", err);
		goto err_free_rx;
	}

	/*
	 * Configure MAC TX
	 */
	i210_configure_tx(adapter);

	/*
	 * Enable TX
	 */
	i210_enable_tx(adapter);

	/*
	 * Configure MAC RX
	 */
	i210_configure_rx(adapter);

	/*
	 * Wait for PHY link
	 *
	 * This is temporary for the learning/debugging stage
	 * Later link handling will become asynchronous
	 */
	err = i210_phy_wait_link(adapter, 5000);
	if (err) {
		if (err == -ENOLINK) {
			pr_info(DRV_NAME
				": no PHY link\n");
		} else {
			pr_err(DRV_NAME
			       ": PHY link check failed: %d\n",
			       err);
		}

		goto err_free_rx;
	}

	/*
	 * PHY link is up
	 * Allow the network stack to transmit
	 */
	netif_start_queue(netdev);

	pr_info(DRV_NAME ": hardware initialization completed\n");

	return 0;

err_free_rx:
	i210_free_rx_resources(adapter);

err_free_tx:
	i210_free_tx_resources(adapter);

	return err;
}

static int i210_close(struct net_device *netdev)
{
	struct i210_adapter *adapter = netdev_priv(netdev);

	pr_info(DRV_NAME ": i210_close()\n");

	/* Stop accepting new packets from the network stack. */
	netif_stop_queue(netdev);

	/*
	 * Now it is safe to release the rings and RX buffers.
	 */
	i210_free_rx_resources(adapter);
	i210_free_tx_resources(adapter);

	return 0;
}

static netdev_tx_t i210_xmit(struct sk_buff *skb,
                             struct net_device *netdev)
{
    struct i210_adapter *adapter = netdev_priv(netdev);
    struct i210_tx_ring *tx_ring = &adapter->tx_ring;
    struct i210_tx_desc *tx_desc;
    struct i210_tx_buffer *tx_buffer;
    dma_addr_t dma;
    u16 i;
    u32 cmd;

	pr_info(DRV_NAME ": i210_xmit() skb_len=%u\n", skb->len);

	if (i210_tx_ring_full(tx_ring)) {
		netif_stop_queue(netdev);
		return NETDEV_TX_BUSY;
	}

    i = tx_ring->next_to_use;

    tx_desc = &tx_ring->desc[i];
    tx_buffer = &tx_ring->buffer_info[i];

    dma = dma_map_single(
	&adapter->pdev->dev,
	skb->data,
	skb->len,
	DMA_TO_DEVICE
	);

    if (dma_mapping_error(&adapter->pdev->dev, dma)) {
        dev_kfree_skb_any(skb);
        return NETDEV_TX_OK;
    }

    tx_buffer->skb = skb;
    tx_buffer->dma = dma;
    tx_buffer->len = skb->len;

    tx_desc->buffer_addr = cpu_to_le64(dma);

    cmd = skb->len;
	cmd |= I210_TXD_DTYP_ADV << I210_TXD_DTYP_SHIFT;
	cmd |= (I210_TXD_CMD_EOP |
			I210_TXD_CMD_IFCS |
			I210_TXD_CMD_RS |
			I210_TXD_CMD_DEXT) << I210_TXD_CMD_SHIFT;

    tx_desc->cmd_type_len = cpu_to_le32(cmd);
    tx_desc->olinfo_status = 0;

    wmb();

    i++;
    if (i == tx_ring->count)
        i = 0;

    tx_ring->next_to_use = i;
    writel(i, adapter->hw.hw_addr + I210_TDT(0));
	udelay(10);
	i210_clean_tx(adapter);
	i210_clean_rx(adapter, 64);

	pr_info(DRV_NAME ": TX ring: TDH=%u TDT=%u\n",
	readl(adapter->hw.hw_addr + I210_TDH(0)),
	readl(adapter->hw.hw_addr + I210_TDT(0)));

	udelay(10);

    return NETDEV_TX_OK;
}

static void i210_get_drvinfo(struct net_device *netdev,
			     struct ethtool_drvinfo *drvinfo)
{
	struct i210_adapter *adapter = netdev_priv(netdev);

	strlcpy(drvinfo->driver, DRV_NAME, sizeof(drvinfo->driver));
	strlcpy(drvinfo->bus_info, pci_name(adapter->pdev), sizeof(drvinfo->bus_info));

	// pr_info(DRV_NAME ": i210_get_drvinfo()\n");
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
			goto err_napi;

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