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
	tctl |= I210_TCTL_CT;

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

	writel(lower_32_bits(rx_ring->dma),hw + I210_RDBAL(0));
	writel(upper_32_bits(rx_ring->dma), hw + I210_RDBAH(0));
	writel(rx_ring->size, hw + I210_RDLEN(0));

	writel(0, hw + I210_SRRCTL(0));
	writel(0, hw + I210_RDH(0));
	writel(0, hw + I210_RDT(0));

	rxdctl = readl(hw + I210_RXDCTL(0));
	rxdctl |= I210_RXDCTL_ENABLE;

	writel(rxdctl, hw + I210_RXDCTL(0));

	do {
		rxdctl = readl(hw + I210_RXDCTL(0));
		if (rxdctl & I210_RXDCTL_ENABLE)
			break;
		udelay(1);
	} while (--timeout);

	if (!(rxdctl & I210_RXDCTL_ENABLE))
		return -ETIMEDOUT;

	/*
	 * Make descriptors visible to the device
	 * before giving the ring to hardware
	 */
	dma_wmb();

	/*
	 * Give descriptors 0..count-1 to hardware
	 */
	writel(rx_ring->count - 1, hw + I210_RDT(0));

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

	while (work_done < budget) {
		struct i210_rx_desc *rx_desc;
		struct i210_rx_buffer *rx_buffer;
		struct sk_buff *skb;
		u16 i;
		u16 length;
		u8 status;

		i = rx_ring->next_to_clean;

		rx_desc = &rx_ring->desc[i];
		rx_buffer = &rx_ring->buffer_info[i];

		status = READ_ONCE(rx_desc->status);
		if (!(status & I210_RXD_STAT_DD))
			break;

		dma_rmb();

		length = le16_to_cpu(READ_ONCE(rx_desc->length));

		skb = rx_buffer->skb;
		if (!skb) {
			pr_err(DRV_NAME ": RX desc[%u] has no skb\n", i);
			break;
		}

		if (!(status & I210_RXD_STAT_EOP)) {
			pr_err(DRV_NAME ": RX desc[%u] is not EOP\n", i);

			dma_unmap_single(
				&pdev->dev,
				rx_buffer->dma,
				I210_RX_BUFFER_SIZE,
				DMA_FROM_DEVICE
			);

			rx_buffer->skb = NULL;
			rx_buffer->dma = 0;

			dev_kfree_skb_any(skb);

			goto refill;
		}

		dma_unmap_single(
			&pdev->dev,
			rx_buffer->dma,
			I210_RX_BUFFER_SIZE,
			DMA_FROM_DEVICE
		);

		rx_buffer->skb = NULL;
		rx_buffer->dma = 0;

		skb_put(skb, length);

		skb->protocol = eth_type_trans(skb, adapter->netdev);

		netif_receive_skb(skb);

refill:
		if (i210_alloc_rx_buffer(adapter, i)) {
			pr_err(DRV_NAME ": RX refill failed for desc[%u]\n", i);
			break;
		}

		dma_wmb();

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
	rx_ring->desc[i].length = 0;
	rx_ring->desc[i].status = 0;
	rx_ring->desc[i].errors = 0;

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
    int timeout = 1000;

    pr_info(DRV_NAME ": configuring TX ring\n");

    /*
     * Queue must be disabled while configuring base/length.
     */
    writel(0, hw + I210_TXDCTL(0));
    writel(lower_32_bits(tx_ring->dma), hw + I210_TDBAL(0));
    writel(upper_32_bits(tx_ring->dma), hw + I210_TDBAH(0));
    writel(tx_ring->size, hw + I210_TDLEN(0));

    writel(0, hw + I210_TDH(0));
    writel(0, hw + I210_TDT(0));

    /*
     * Intel recommended write-back policy:
     *
     * WTHRESH = 1
     * PTHRESH = 0
     * HTHRESH = 0
     */
    txdctl = I210_TXDCTL_WTHRESH;

    /*
     * Enable queue.
     */
    txdctl |= I210_TXDCTL_ENABLE;
    writel(txdctl, hw + I210_TXDCTL(0));
    do {
        txdctl = readl(hw + I210_TXDCTL(0));
        if (txdctl & I210_TXDCTL_ENABLE)
            break;
        udelay(1);
    } while (--timeout);

    if (!(txdctl & I210_TXDCTL_ENABLE)) {
        pr_err(DRV_NAME ": TX queue failed to enable\n");
        return -ETIMEDOUT;
    }

    pr_info(DRV_NAME ": TXDCTL = 0x%08x\n", txdctl);

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
	static const char * const phy_reg_names[32] = {
		[0]  = "BMCR",
		[1]  = "BMSR",
		[2]  = "PHYID1",
		[3]  = "PHYID2",
		[4]  = "ANAR",
		[5]  = "ANLPAR",
		[6]  = "ANER",
		[7]  = "ANNPTR",
		[8]  = "ANLPNP",
		[9]  = "1000BASE-T CTRL",
		[10] = "1000BASE-T STATUS",
		[11] = "EXT STATUS",
		[12] = "PHY CTRL",
		[13] = "PHY STATUS",
		[14] = "PHY INT",
		[15] = "PHY INT MASK",
		[16] = "PHY REG16",
		[17] = "PHY REG17",
		[18] = "PHY REG18",
		[19] = "PHY REG19",
		[20] = "PHY REG20",
		[21] = "PHY REG21",
		[22] = "PHY REG22",
		[23] = "PHY REG23",
		[24] = "PHY REG24",
		[25] = "PHY REG25",
		[26] = "PHY REG26",
		[27] = "PHY REG27",
		[28] = "PHY REG28",
		[29] = "PHY REG29",
		[30] = "PHY REG30",
		[31] = "PHY REG31",
	};

	int i;
	int err;
	u16 value;

	pr_info(DRV_NAME ": ================= PHY DUMP =================\n");

	for (i = 0; i < 32; i++) {
		err = i210_read_phy_reg(adapter, i, &value);

		if (err) {
			pr_err(DRV_NAME ": PHY[%02d] %-20s READ ERROR %d\n",
			       i,
			       phy_reg_names[i] ? phy_reg_names[i] : "",
			       err);
			continue;
		}

		pr_info(DRV_NAME ": PHY[%02d] %-20s = 0x%04x\n",
			i,
			phy_reg_names[i] ? phy_reg_names[i] : "",
			value);
	}

	pr_info(DRV_NAME ": ============================================\n");

	return 0;
}


int i210_phy_power_up(struct i210_adapter *adapter)
{
	u16 ctrl;
	int err;

	err = i210_read_phy_reg(adapter, 0, &ctrl);
	if (err) {
		pr_err(DRV_NAME ": failed to read PHY BMCR: %d\n", err);
		return err;
	}

	pr_info(DRV_NAME ": PHY BMCR before power-up = 0x%04x\n", ctrl);

	ctrl &= ~BIT(11); /* POWER DOWN */
	ctrl &= ~BIT(10); /* ISOLATE */

	err = i210_write_phy_reg(adapter, 0, ctrl);
	if (err) {
		pr_err(DRV_NAME ": failed to write PHY BMCR: %d\n", err);
		return err;
	}

	pr_info(DRV_NAME ": PHY BMCR after power-up = 0x%04x\n", ctrl);

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
	 * Bring PHY out of power-down and isolate modes.
	 */
	err = i210_phy_power_up(adapter);
	if (err) {
		pr_err(DRV_NAME ": PHY power-up failed: %d\n", err);
		return err;
	}

	/*
	 * Dump PHY state after power-up, before autonegotiation.
	 */
	err = i210_dump_phy(adapter);
	if (err)
		return err;

	/*
	 * Start PHY auto-negotiation.
	 */
	err = i210_phy_start_autoneg(adapter);
	if (err) {
		pr_err(DRV_NAME ": PHY autonegotiation start failed: %d\n", err);
		return err;
	}

	return 0;
}


void i210_configure_tx_buffer(struct i210_adapter *adapter)
{
    void __iomem *hw = adapter->hw.hw_addr;

    writel(I210_TXPBSIZE_DEFAULT, hw + I210_TXPBS);
    writel(I210_DTXMXPKTSZ_DEFAULT, hw + I210_DTXMXPKTSZ);

    pr_info(DRV_NAME ": TXPBS=0x%08x DTXMXPKTSZ=0x%08x\n",
		readl(hw + I210_TXPBS), readl(hw + I210_DTXMXPKTSZ));
}


int i210_alloc_irq(struct i210_adapter *adapter)
{
	struct pci_dev *pdev = adapter->pdev;
	int ret;

	ret = pci_alloc_irq_vectors(pdev, 1, 1, PCI_IRQ_MSI);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to allocate MSI IRQ vector: %d\n", ret);
		return ret;
	}

	dev_info(&pdev->dev,
		"IRQ allocated: vector=%d irq=%d MSI=%d MSI-X=%d\n",
		0,
		pci_irq_vector(pdev, 0),
		pdev->msi_enabled,
		pdev->msix_enabled
	);

	adapter->irq_vectors = ret;
	adapter->irq = pci_irq_vector(pdev, 0);
	if (adapter->irq < 0) {
		dev_err(&pdev->dev,
			"failed to get IRQ vector: %d\n",
			adapter->irq);

		pci_free_irq_vectors(pdev);
		adapter->irq_vectors = 0;

		return adapter->irq;
	}

	dev_info(&pdev->dev,
		 "IRQ allocated: vector=0 irq=%d MSI=%d MSI-X=%d\n",
		 adapter->irq,
		 pdev->msi_enabled,
		 pdev->msix_enabled);

	return 0;
}


void i210_configure_interrupts(struct i210_adapter *adapter)
{
	void __iomem *hw = adapter->hw.hw_addr;
	u32 ivar;

	/* RX queue 0 -> EICR bit 0 */
	// ivar = I210_IVAR_RX0_VALID | (0 << I210_IVAR_RX0_SHIFT);
	ivar = 0;
	ivar |= 0 << I210_IVAR_RX0_SHIFT;
	ivar |= I210_IVAR_RX0_VALID;
	writel(ivar, hw + I210_IVAR);

	/* Enable RX queue 0 interrupt */
	writel(I210_EICR_RXTXQ0, hw + I210_EIMS);

	dev_info(&adapter->pdev->dev,
		"interrupts configured: IVAR=0x%08x EIMS=0x%08x\n",
		readl(hw + I210_IVAR),
		readl(hw + I210_EIMS));
}


void i210_disable_interrupts(struct i210_adapter *adapter)
{
	void __iomem *hw = adapter->hw.hw_addr;

	writel(0xffffffff, hw + I210_EIMC);
	writel(0xffffffff, hw + I210_IMC);

	/* Clear pending causes */
	readl(hw + I210_EICR);
	readl(hw + I210_ICR);
}


irqreturn_t i210_interrupt(int irq, void *data)
{
    struct i210_adapter *adapter = data;
    void __iomem *hw = adapter->hw.hw_addr;
    u32 eicr;

    eicr = readl(hw + I210_EICR);

    if (!eicr)
        return IRQ_NONE;

    pr_info(DRV_NAME
		": IRQ %d: EICR=0x%08x EIMS=0x%08x RDH=%u RDT=%u\n",
		irq,
		eicr,
		readl(hw + I210_EIMS),
		readl(hw + I210_RDH(0)),
		readl(hw + I210_RDT(0))
	);

    /*
     * Disable RX/TX queue interrupts while NAPI is running
     */
    writel(I210_EICR_RXTXQ0, hw + I210_EIMC);

    /*
     * Clear interrupt cause
     */
    writel(eicr, hw + I210_EICR);

    napi_schedule(&adapter->napi);

    return IRQ_HANDLED;
}

int i210_request_irq(struct i210_adapter *adapter)
{
	int ret;

	ret = request_irq(
		adapter->irq,
		i210_interrupt,
		0,
		DRV_NAME,
		adapter
	);

	if (ret) {
		dev_err(&adapter->pdev->dev,
			"request_irq(%d) failed: %d\n",
			adapter->irq,
			ret);
		return ret;
	}

	dev_info(&adapter->pdev->dev, "IRQ %d requested\n", adapter->irq);

	return 0;
}