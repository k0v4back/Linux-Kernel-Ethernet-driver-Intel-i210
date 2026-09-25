#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>

#include "i210_helpers.h"

int i210_read_nvm_word(struct i210_adapter *adapter, u16 offset, u16 *data)
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
int i210_reset_hw(struct i210_adapter *adapter)
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

	/*
	 * Datasheet:
	 * wait at least 3 ms after CTRL.RST.
	 */
	msleep(3);

	ctrl = readl(hw + I210_CTRL);
	status = readl(hw + I210_STATUS);
	pr_info(DRV_NAME ": CTRL after reset = 0x%08x\n", ctrl);
	pr_info(DRV_NAME ": STATUS after reset = 0x%08x\n", status);

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
void i210_set_mac_address(struct i210_adapter *adapter, const u8 *mac)
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


void i210_configure_tx(struct i210_adapter *adapter)
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


void i210_configure_rx(struct i210_adapter *adapter)
{
	void __iomem *hw = adapter->hw.hw_addr;
	u32 rctl;

	rctl = readl(hw + I210_RCTL);

    /*
     * Enable receiver.
     */
    rctl |= I210_RCTL_EN;

    /*
     * Accept broadcast packets.
     */
    rctl |= I210_RCTL_BAM;

    /*
     * Strip Ethernet CRC.
     */
    rctl |= I210_RCTL_SECRC;

	writel(rctl, hw + I210_RCTL);
	pr_info(DRV_NAME ": RCTL = 0x%08x\n", rctl);
}


int i210_setup_rx_resources(struct i210_adapter *adapter)
{
	struct i210_rx_ring *rx_ring = &adapter->rx_ring;
	struct pci_dev *pdev = adapter->pdev;
	unsigned int i;

	pr_info(DRV_NAME ": setting up RX resources\n");

	rx_ring->count = I210_RX_DESC_COUNT;
	rx_ring->size = rx_ring->count * sizeof(struct i210_rx_desc);

	if (rx_ring->size & 0x7f) {
		pr_err(DRV_NAME ": invalid RX ring size: %u\n", rx_ring->size);
		return -EINVAL;
	}

	rx_ring->buffer_info = kcalloc(
		rx_ring->count,
		sizeof(*rx_ring->buffer_info),
		GFP_KERNEL
	);

	if (!rx_ring->buffer_info) {
		pr_err(DRV_NAME ": failed to allocate RX buffer info\n");
		return -ENOMEM;
	}

	rx_ring->desc = dma_alloc_coherent(
		&pdev->dev,
		rx_ring->size,
		&rx_ring->dma,
		GFP_KERNEL
	);

	if (!rx_ring->desc) {
		pr_err(DRV_NAME ": failed to allocate RX descriptor ring\n");
		kfree(rx_ring->buffer_info);
		rx_ring->buffer_info = NULL;
		return -ENOMEM;
	}

	memset(rx_ring->desc, 0, rx_ring->size);

	rx_ring->next_to_clean = 0;

	BUILD_BUG_ON(sizeof(struct i210_rx_desc) != 16);

	/*
	 * Allocate and map RX buffers.
	 */
	for (i = 0; i < rx_ring->count; i++) {
		struct sk_buff *skb;
		dma_addr_t dma;

		skb = netdev_alloc_skb(adapter->netdev, I210_RX_BUFFER_SIZE);
		if (!skb) {
			pr_err(DRV_NAME ": failed to allocate RX skb[%u]\n", i);
			goto err_free_buffers;
		}

		dma = dma_map_single(
			&pdev->dev,
			skb->data,
			I210_RX_BUFFER_SIZE,
			DMA_FROM_DEVICE
		);

		if (dma_mapping_error(&pdev->dev, dma)) {
			pr_err(DRV_NAME ": failed to DMA map RX skb[%u]\n", i);
			dev_kfree_skb_any(skb);
			goto err_free_buffers;
		}

		rx_ring->buffer_info[i].skb = skb;
		rx_ring->buffer_info[i].dma = dma;

		rx_ring->desc[i].buffer_addr = cpu_to_le64(dma);
	}

	pr_info(DRV_NAME ": RX ring allocated\n");
	pr_info(DRV_NAME ": descriptors      = %u\n", rx_ring->count);
	pr_info(DRV_NAME ": descriptor size  = %zu\n", sizeof(struct i210_rx_desc));
	pr_info(DRV_NAME ": ring size        = %u bytes\n", rx_ring->size);
	pr_info(DRV_NAME ": CPU address      = %px\n", rx_ring->desc);
	pr_info(DRV_NAME ": DMA address      = %pad\n", &rx_ring->dma);

	pr_info(DRV_NAME ": RX resources ready\n");

	return 0;

err_free_buffers:
	while (i--) {
		struct i210_rx_buffer *rx_buffer;

		rx_buffer = &rx_ring->buffer_info[i];

		if (rx_buffer->skb) {
			dma_unmap_single(
				&pdev->dev,
				rx_buffer->dma,
				I210_RX_BUFFER_SIZE,
				DMA_FROM_DEVICE
			);

			dev_kfree_skb_any(rx_buffer->skb);

			rx_buffer->skb = NULL;
			rx_buffer->dma = 0;
		}
	}

	dma_free_coherent(
		&pdev->dev,
		rx_ring->size,
		rx_ring->desc,
		rx_ring->dma
	);

	rx_ring->desc = NULL;
	rx_ring->dma = 0;

	kfree(rx_ring->buffer_info);
	rx_ring->buffer_info = NULL;

	return -ENOMEM;
}


int i210_configure_rx_ring(struct i210_adapter *adapter)
{
	struct i210_rx_ring *rx_ring = &adapter->rx_ring;
	void __iomem *hw = adapter->hw.hw_addr;
	u32 rxdctl;
	int timeout = 1000;

	pr_info(DRV_NAME ": configuring RX ring\n");

	writel(lower_32_bits(rx_ring->dma), hw + I210_RDBAL(0));
	writel(upper_32_bits(rx_ring->dma), hw + I210_RDBAH(0));
	writel(rx_ring->size, hw + I210_RDLEN(0));

	writel(0, hw + I210_SRRCTL(0));

	/*
	 * Hardware starts from descriptor 0.
	 */
	writel(0, hw + I210_RDH(0));

	/*
	 * Give all descriptors to hardware.
	 */
	// writel(rx_ring->count - 1, hw + I210_RDT(0));
	writel(0, hw + I210_RDT(0));
	pr_info(DRV_NAME ": RX ring before enable: RDH=%u RDT=%u\n",
		readl(hw + I210_RDH(0)), readl(hw + I210_RDT(0))
	);

	rxdctl = readl(hw + I210_RXDCTL(0));
	pr_info(DRV_NAME ": RXDCTL before enable = 0x%08x\n", rxdctl);

	/*
	 * RXDCTL.ENABLE
	 */
	rxdctl |= I210_RXDCTL_ENABLE;
	writel(rxdctl, hw + I210_RXDCTL(0));

	/*
	 * Wait until hardware reports queue enabled.
	 */
	do {
		rxdctl = readl(hw + I210_RXDCTL(0));
		if (rxdctl & I210_RXDCTL_ENABLE)
			break;
		udelay(1);
	} while (--timeout);

	if (!(rxdctl & I210_RXDCTL_ENABLE)) {
		pr_err(DRV_NAME ": RX queue failed to enable\n");
		return -ETIMEDOUT;
	}

	pr_info(DRV_NAME ": RXDCTL after enable = 0x%08x\n", rxdctl);

	dma_wmb();

	/*
	 * NOW give descriptors to hardware.
	 */
	writel(rx_ring->count - 1, hw + I210_RDT(0));

	pr_info(
		DRV_NAME
		": RX ring regs: "
		"RDBAL=0x%08x "
		"RDBAH=0x%08x "
		"RDLEN=0x%08x "
		"RDH=%u "
		"RDT=%u "
		"RXDCTL=0x%08x\n",
		readl(hw + I210_RDBAL(0)),
		readl(hw + I210_RDBAH(0)),
		readl(hw + I210_RDLEN(0)),
		readl(hw + I210_RDH(0)),
		readl(hw + I210_RDT(0)),
		readl(hw + I210_RXDCTL(0))
	);

	return 0;
}


void i210_free_rx_resources(struct i210_adapter *adapter)
{
	struct i210_rx_ring *rx_ring = &adapter->rx_ring;
	struct pci_dev *pdev = adapter->pdev;
	unsigned int i;

	pr_info(DRV_NAME ": freeing RX resources\n");

	if (rx_ring->buffer_info) {
		for (i = 0; i < rx_ring->count; i++) {
			struct i210_rx_buffer *rx_buffer = &rx_ring->buffer_info[i];

			if (!rx_buffer->skb)
				continue;

			dma_unmap_single(
				&pdev->dev,
				rx_buffer->dma,
				I210_RX_BUFFER_SIZE,
				DMA_FROM_DEVICE
			);

			dev_kfree_skb_any(rx_buffer->skb);

			rx_buffer->skb = NULL;
			rx_buffer->dma = 0;
		}
	}

	if (rx_ring->desc) {
		dma_free_coherent(
			&pdev->dev,
			rx_ring->size,
			rx_ring->desc,
			rx_ring->dma
		);

		rx_ring->desc = NULL;
		rx_ring->dma = 0;
	}

	kfree(rx_ring->buffer_info);
	rx_ring->buffer_info = NULL;

	rx_ring->count = 0;
	rx_ring->size = 0;
	rx_ring->next_to_clean = 0;
}


int i210_clean_rx(struct i210_adapter *adapter, int budget)
{
	struct i210_rx_ring *rx_ring = &adapter->rx_ring;
	struct pci_dev *pdev = adapter->pdev;
	void __iomem *hw = adapter->hw.hw_addr;
	int work_done = 0;
	u32 status_reg;

	status_reg = readl(hw + I210_STATUS);

	pr_info(DRV_NAME ": RX state: STATUS=0x%08x RDH=%u RDT=%u RXDCTL=0x%08x RCTL=0x%08x\n",
		status_reg,
		readl(hw + I210_RDH(0)),
		readl(hw + I210_RDT(0)),
		readl(hw + I210_RXDCTL(0)),
		readl(hw + I210_RCTL));

	while (work_done < budget) {
		u16 i;
		u16 length;
		u8 status;

		struct i210_rx_desc *rx_desc;
		struct i210_rx_buffer *rx_buffer;
		struct sk_buff *skb;

		i = rx_ring->next_to_clean;
		rx_desc = &rx_ring->desc[i];
		rx_buffer = &rx_ring->buffer_info[i];

		status = READ_ONCE(rx_desc->status);
		if (!(status & I210_RXD_STAT_DD))
			break;

		dma_rmb();

		length = le16_to_cpu(READ_ONCE(rx_desc->length));
		pr_info(
			DRV_NAME
			": RX desc[%u]: "
			"status=0x%02x "
			"length=%u "
			"errors=0x%02x\n",
			i,
			status,
			length,
			rx_desc->errors
		);

		skb = rx_buffer->skb;
		if (!skb) {
			pr_err(DRV_NAME
			       ": RX desc[%u] has no skb\n",
			       i);
			break;
		}

		dma_unmap_single(
			&pdev->dev,
			rx_buffer->dma,
			I210_RX_BUFFER_SIZE,
			DMA_FROM_DEVICE
		);

		rx_buffer->dma = 0;
		rx_buffer->skb = NULL;

		/*
		 * For the first version we expect
		 * one descriptor per packet.
		 */
		if (!(status & I210_RXD_STAT_EOP)) {
			pr_err(
				DRV_NAME
				": RX desc[%u]: "
				"packet is not EOP\n",
				i
			);

			dev_kfree_skb_any(skb);
			goto refill;
		}

		skb_put(skb, length);

		skb->protocol = eth_type_trans(skb, adapter->netdev);
		pr_info(
			DRV_NAME
			": RX packet received: "
			"len=%u protocol=0x%04x\n",
			skb->len,
			ntohs(skb->protocol)
		);

		netif_receive_skb(skb);

refill:
		if (i210_alloc_rx_buffer(adapter, i)) {
			pr_err(
				DRV_NAME
				": failed to refill RX desc[%u]\n",
				i
			);
			break;
		}

		/*
		 * Return descriptor to hardware.
		 */
		writel(i, hw + I210_RDT(0));

		rx_ring->next_to_clean++;
		if (rx_ring->next_to_clean == rx_ring->count)
			rx_ring->next_to_clean = 0;

		work_done++;
	}

	return work_done;
}


int i210_alloc_rx_buffer(struct i210_adapter *adapter, u16 i)
{
	struct i210_rx_ring *rx_ring = &adapter->rx_ring;
	struct i210_rx_buffer *rx_buffer;
	struct sk_buff *skb;
	dma_addr_t dma;

	rx_buffer = &rx_ring->buffer_info[i];

	skb = netdev_alloc_skb(adapter->netdev, I210_RX_BUFFER_SIZE);
	if (!skb)
		return -ENOMEM;

	dma = dma_map_single(
		&adapter->pdev->dev,
		skb->data,
		I210_RX_BUFFER_SIZE,
		DMA_FROM_DEVICE
	);

	if (dma_mapping_error(&adapter->pdev->dev, dma)) {
		dev_kfree_skb_any(skb);
		return -ENOMEM;
	}

	rx_buffer->skb = skb;
	rx_buffer->dma = dma;

	rx_ring->desc[i].buffer_addr = cpu_to_le64(dma);
	rx_ring->desc[i].status = 0;

	return 0;
}


/*
 *  i210_setup_tx_resources - allocate Tx resources (Descriptors)
 *
 *  Similar int igb_setup_tx_resources(struct igb_ring *tx_ring)
 */
int i210_setup_tx_resources(struct i210_adapter *adapter)
{
	struct i210_tx_ring *tx_ring = &adapter->tx_ring;
	struct pci_dev *pdev = adapter->pdev;
	size_t size;

	pr_info(DRV_NAME ": setting up TX resources\n");

	tx_ring->count = I210_TX_DESC_COUNT;
	tx_ring->size = tx_ring->count * sizeof(struct i210_tx_desc);

	/*
	 * TDLEN must be 128-byte aligned
	 * 64 descriptors * 16 bytes = 1024 bytes, already aligned
	 */
	if (tx_ring->size & 0x7f) {
		pr_err(DRV_NAME ": invalid TX ring size: %u\n", tx_ring->size);
		return -EINVAL;
	}

	/*
	 * Allocate software information for each descriptor
	 * This is CPU-only memory, NIC never see it
	 */
	tx_ring->buffer_info = kcalloc(
		tx_ring->count, 
		sizeof(*tx_ring->buffer_info), 
		GFP_KERNEL
	);
	if (!tx_ring->buffer_info) {
		pr_err(DRV_NAME ": failed to allocate TX buffer info\n");
		return -ENOMEM;
	}

	/*
	 * Allocate DMA-coherent memory for descriptor ring
	 *
	 * tx_ring->desc: CPU virtual address
	 * tx_ring->dma: DMA address visible for I210
	 */
	tx_ring->desc = dma_alloc_coherent(
		&pdev->dev,
		tx_ring->size,
		&tx_ring->dma,
		GFP_KERNEL
	);
	if (!tx_ring->desc) {
		pr_err(DRV_NAME ": failed to allocate TX descriptor ring\n");
		kfree(tx_ring->buffer_info);
		tx_ring->buffer_info = NULL;
		return -ENOMEM;
	}

	memset(tx_ring->desc, 0, tx_ring->size);

	/*
	 * Software producer/consumer indexes.
	 */
	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;

	pr_info(DRV_NAME ": TX ring allocated\n");
	pr_info(DRV_NAME ": descriptors      = %u\n", tx_ring->count);
	pr_info(DRV_NAME ": descriptor size  = %zu\n", sizeof(struct i210_tx_desc));
	pr_info(DRV_NAME ": ring size        = %u bytes\n", tx_ring->size);
	pr_info(DRV_NAME ": CPU address      = %px\n", tx_ring->desc);
	pr_info(DRV_NAME ": DMA address      = %pad\n", &tx_ring->dma);

	pr_info(DRV_NAME ": TX resources ready\n");

	return 0;
}


/*
 *  i210_free_tx_resources - Free Tx Resources per Queue
 *
 * Setup TX resources:
 * - allocate descriptors
 * - allocate buffer_info
 *
 *  Similar int igb_free_tx_resources(struct igb_ring *tx_ring)
 */
void i210_free_tx_resources(struct i210_adapter *adapter)
{
	struct i210_tx_ring *tx_ring = &adapter->tx_ring;
	struct pci_dev *pdev = adapter->pdev;

	pr_info(DRV_NAME ": freeing TX resources\n");

	if (tx_ring->desc) {
		dma_free_coherent(
			&pdev->dev,
			tx_ring->size,
			tx_ring->desc,
			tx_ring->dma
		);
		tx_ring->desc = NULL;
		tx_ring->dma = 0;
	}

	kfree(tx_ring->buffer_info);
	tx_ring->buffer_info = NULL;

	tx_ring->count = 0;
	tx_ring->size = 0;
	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;
}


int i210_configure_tx_ring(struct i210_adapter *adapter)
{
	struct i210_tx_ring *tx_ring = &adapter->tx_ring;
	void __iomem *hw = adapter->hw.hw_addr;
	u32 txdctl;

	pr_info(DRV_NAME ": configuring TX ring\n");

	writel(lower_32_bits(tx_ring->dma), hw + I210_TDBAL(0));
	writel(upper_32_bits(tx_ring->dma), hw + I210_TDBAH(0));
	writel(tx_ring->size, hw + I210_TDLEN(0));

	writel(0, hw + I210_TDH(0));
	writel(0, hw + I210_TDT(0));

	txdctl = readl(hw + I210_TXDCTL(0));
	pr_info(DRV_NAME ": TXDCTL before enable = 0x%08x\n", txdctl);

	txdctl |= I210_TXDCTL_ENABLE;
	writel(txdctl, hw + I210_TXDCTL(0));

	txdctl = readl(hw + I210_TXDCTL(0));
	pr_info(DRV_NAME ": TXDCTL after enable = 0x%08x\n", txdctl);

	/* Check realy enable or not */
	if (!(txdctl & I210_TXDCTL_ENABLE)) {
		pr_err(DRV_NAME ": TX queue failed to enable\n");
		return -ETIMEDOUT;
	}

	return 0;
}


void i210_enable_tx(struct i210_adapter *adapter)
{
    void __iomem *hw = adapter->hw.hw_addr;
    u32 tctl;

    tctl = readl(hw + I210_TCTL);
    tctl |= I210_TCTL_EN;
    writel(tctl, hw + I210_TCTL);
    pr_info(DRV_NAME ": TCTL = 0x%08x\n", readl(hw + I210_TCTL));
}


bool i210_tx_ring_full(struct i210_tx_ring *tx_ring)
{
	u16 next;
	next = tx_ring->next_to_use + 1;
	if (next == tx_ring->count)
		next = 0;

	return next == tx_ring->next_to_clean;
}


void i210_clean_tx(struct i210_adapter *adapter)
{
	struct i210_tx_ring *tx_ring = &adapter->tx_ring;
	struct device *dev = &adapter->pdev->dev;

	while (tx_ring->next_to_clean != tx_ring->next_to_use) {
		u16 i = tx_ring->next_to_clean;
		struct i210_tx_desc *tx_desc = &tx_ring->desc[i];
		struct i210_tx_buffer *tx_buffer = &tx_ring->buffer_info[i];
		u32 status;

		status = le32_to_cpu(tx_desc->olinfo_status);

		if (!(status & I210_TXD_STAT_DD))
			break;

		pr_info(DRV_NAME
			": cleaning TX desc[%u]: status=0x%08x\n",
			i, status);

		if (tx_buffer->skb) {
			dma_unmap_single(dev,
					 tx_buffer->dma,
					 tx_buffer->len,
					 DMA_TO_DEVICE);

			dev_kfree_skb_any(tx_buffer->skb);

			tx_buffer->skb = NULL;
			tx_buffer->dma = 0;
			tx_buffer->len = 0;
		}

		tx_desc->olinfo_status = 0;

		tx_ring->next_to_clean++;

		if (tx_ring->next_to_clean == tx_ring->count)
			tx_ring->next_to_clean = 0;
	}

	if (netif_queue_stopped(adapter->netdev))
		netif_wake_queue(adapter->netdev);
}


int i210_read_phy_reg(struct i210_adapter *adapter, u8 reg, u16 *value)
{
	void __iomem *hw = adapter->hw.hw_addr;
	u32 mdic;
	int timeout = 1000;

	/*
	 * Make sure previous MDI transaction is finished
	 * and clear any previous error
	 */
	mdic = readl(hw + I210_MDIC);
	if (mdic & I210_MDIC_ERROR) {
		pr_warn(DRV_NAME ": MDIC error before read: 0x%08x\n", mdic);
	}

	/*
	 * PHY register read.
	 */
	mdic = I210_MDIC_OP_READ | ((u32)reg << I210_MDIC_REG_SHIFT);
	writel(mdic, hw + I210_MDIC);

	/*
	 * Wait until PHY transaction completes
	 */
	do {
		mdic = readl(hw + I210_MDIC);
		if (mdic & I210_MDIC_READY)
			break;
		udelay(1);
	} while (--timeout);

	if (!(mdic & I210_MDIC_READY)) {
		pr_err(DRV_NAME ": MDIC read timeout, reg=%u\n", reg);
		return -ETIMEDOUT;
	}

	if (mdic & I210_MDIC_ERROR) {
		pr_err(DRV_NAME ": MDIC read error, reg=%u MDIC=0x%08x\n", reg, mdic);
		return -EIO;
	}

	*value = mdic & I210_MDIC_DATA_MASK;

	return 0;
}


int i210_write_phy_reg(struct i210_adapter *adapter,
                       u16 reg,
                       u16 value)
{
	void __iomem *hw = adapter->hw.hw_addr;
	u32 mdic;
	int timeout = 1000;

	/*
	 * MDIC:
	 *
	 * bits 15:0  = data
	 * bits 20:16 = PHY register address
	 * bits 27:26 = operation
	 * bit 28     = ready
	 * bit 30     = error
	 */
	mdic = value;
	mdic |= ((u32)reg << 16);

	/*
	 * OP = 01b -> PHY write
	 */
	mdic |= (1U << 26);
	writel(mdic, hw + I210_MDIC);

	do {
		mdic = readl(hw + I210_MDIC);
		if (mdic & I210_MDIC_READY)
			break;
		udelay(1);
	} while (--timeout);

	if (!(mdic & I210_MDIC_READY)) {
		pr_err(DRV_NAME ": PHY write timeout, reg=%u value=0x%04x\n", reg, value);
		return -ETIMEDOUT;
	}

	if (mdic & I210_MDIC_ERROR) {
		pr_err(DRV_NAME
		       ": PHY write error, reg=%u value=0x%04x MDIC=0x%08x\n",
		       reg, value, mdic);
		return -EIO;
	}

	return 0;
}


int i210_dump_phy(struct i210_adapter *adapter)
{
	u16 ctrl;
	u16 status;
	u16 id1;
	u16 id2;
	u16 an_adv;
	u16 an_lp;
	u16 gb_ctrl;
	int err;

	err = i210_read_phy_reg(adapter, I210_PHY_CTRL, &ctrl);
	if (err)
		return err;

	err = i210_read_phy_reg(adapter, I210_PHY_STATUS, &status);
	if (err)
		return err;

	err = i210_read_phy_reg(adapter, I210_PHY_ID1, &id1);
	if (err)
		return err;

	err = i210_read_phy_reg(adapter, I210_PHY_ID2, &id2);
	if (err)
		return err;

	err = i210_read_phy_reg(adapter, I210_PHY_AN_ADV, &an_adv);
	if (err)
		return err;

	err = i210_read_phy_reg(adapter, I210_PHY_AN_LP_ABILITY, &an_lp);
	if (err)
		return err;

	err = i210_read_phy_reg(adapter, I210_PHY_1000_CTRL, &gb_ctrl);
	if (err)
		return err;

	pr_info(DRV_NAME
		": PHY dump:\n"
		"  CTRL      = 0x%04x\n"
		"  STATUS    = 0x%04x\n"
		"  ID1       = 0x%04x\n"
		"  ID2       = 0x%04x\n"
		"  AN_ADV    = 0x%04x\n"
		"  AN_LP     = 0x%04x\n"
		"  1000_CTRL = 0x%04x\n",
		ctrl,
		status,
		id1,
		id2,
		an_adv,
		an_lp,
		gb_ctrl);

	pr_info(DRV_NAME
		": PHY state: "
		"LINK=%u "
		"AN_ABILITY=%u "
		"AN_COMPLETE=%u\n",
		!!(status & I210_PHY_STATUS_LINK_STATUS),
		!!(status & I210_PHY_STATUS_AN_ABILITY),
		!!(status & I210_PHY_STATUS_AN_COMPLETE));

	return 0;
}


int i210_get_phy_link(struct i210_adapter *adapter, bool *link)
{
	u16 status1;
	u16 status2;
	int err;

	err = i210_read_phy_reg(adapter,
		I210_PHY_STATUS,
		&status1
	);
	if (err)
		return err;

	/*
	 * BMSR link status is commonly latch-low,
	 * so perform a second read.
	 */
	err = i210_read_phy_reg(adapter,
		I210_PHY_STATUS,
		&status2
	);
	if (err)
		return err;

	*link = !!(status2 & I210_PHY_STATUS_LINK_STATUS);
	pr_info(DRV_NAME
		": PHY link: "
		"STATUS1=0x%04x "
		"STATUS2=0x%04x "
		"LINK=%u\n",
		status1,
		status2,
		*link);

	return 0;
}


int i210_phy_start_autoneg(struct i210_adapter *adapter)
{
	u16 ctrl;
	int err;

	err = i210_read_phy_reg(adapter,
		I210_PHY_CTRL,
		&ctrl);
	if (err)
		return err;

	pr_info(DRV_NAME
		": PHY CTRL before autoneg = 0x%04x\n",
		ctrl);

	/*
	 * Enable Auto-Negotiation.
	 */
	ctrl |= I210_PHY_CTRL_AN_ENABLE;

	/*
	 * Restart Auto-Negotiation.
	 */
	ctrl |= I210_PHY_CTRL_RESTART_AN;

	err = i210_write_phy_reg(adapter,
		I210_PHY_CTRL,
		ctrl);
	if (err)
		return err;

	pr_info(DRV_NAME ": PHY CTRL after autoneg = 0x%04x\n", ctrl);

	return 0;
}


int i210_phy_wait_link(struct i210_adapter *adapter, unsigned int timeout_ms)
{
	unsigned int elapsed = 0;
	bool link;
	int err;

	while (elapsed < timeout_ms) {
		err = i210_get_phy_link(adapter, &link);
		if (err)
			return err;

		if (link) {
			pr_info(DRV_NAME ": PHY link is UP after %u ms\n", elapsed);
			return 0;
		}

		msleep(100);
		elapsed += 100;
	}

	pr_info(DRV_NAME ": PHY link is DOWN after %u ms\n", timeout_ms);

	return -ENOLINK;
}


int i210_init_phy(struct i210_adapter *adapter)
{
	u32 ctrl;
	int err;

	pr_info(DRV_NAME ": initializing PHY\n");

	/*
	 * CTRL.SLU must be set for the MAC to recognize
	 * the PHY link indication.
	 */
	ctrl = readl(adapter->hw.hw_addr + I210_CTRL);
	ctrl |= I210_CTRL_SLU;
	writel(ctrl, adapter->hw.hw_addr + I210_CTRL);
	pr_info(DRV_NAME ": MAC CTRL after SLU = 0x%08x\n", ctrl);

	/*
	 * First inspect the PHY state.
	 */
	err = i210_dump_phy(adapter);
	if (err)
		return err;

	/*
	 * Start PHY auto-negotiation.
	 */
	err = i210_phy_start_autoneg(adapter);
	if (err)
		return err;

	return 0;
}