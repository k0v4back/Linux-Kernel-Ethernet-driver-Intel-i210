#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>

#include "i210.h"
#include "i210_defines.h"
#include "i210_debug.h"

void i210_debug_log_tx(struct i210_adapter *adapter,
		       struct sk_buff *skb)
{
	struct ethhdr *eth;

	if (skb->len < ETH_HLEN)
		return;

	eth = (struct ethhdr *)skb->data;
	pr_debug("TX: src=%pM dst=%pM proto=0x%04x len=%u\n",
        eth->h_source,
        eth->h_dest,
        ntohs(eth->h_proto),
        skb->len
    );
}

void i210_debug_log_rx(struct i210_adapter *adapter, u16 index)
{
	struct i210_rx_ring *ring = &adapter->rx_ring;
	struct i210_rx_desc *desc = &ring->desc[index];
	struct sk_buff *skb;
	struct ethhdr *eth;

	if (!(desc->status & I210_RXD_STAT_DD))
		return;

	skb = ring->buffer_info[index].skb;

	if (!skb)
		return;

	if (le16_to_cpu(desc->length) < ETH_HLEN)
		return;

	eth = (struct ethhdr *)skb->data;

	pr_debug("RX: desc=%u src=%pM dst=%pM proto=0x%04x len=%u\n",
        index,
        eth->h_source,
        eth->h_dest,
        ntohs(eth->h_proto),
        le16_to_cpu(desc->length)
    );
}

void i210_debug_dump_rx_ring(struct i210_adapter *adapter)
{
	struct i210_rx_ring *ring = &adapter->rx_ring;
	void __iomem *hw = adapter->hw.hw_addr;
	u16 i;

	pr_debug("RX ring: next_to_clean=%u RDH=%u RDT=%u\n",
        ring->next_to_clean,
        readl(hw + I210_RDH(0)),
        readl(hw + I210_RDT(0))
    );

	for (i = 0; i < ring->count; i++) {
		struct i210_rx_desc *desc = &ring->desc[i];

		pr_debug("RXD[%u]: dma=%016llx len=%u status=0x%02x errors=0x%02x\n",
            i,
            (unsigned long long)
            le64_to_cpu(desc->buffer_addr),
            le16_to_cpu(desc->length),
            READ_ONCE(desc->status),
            READ_ONCE(desc->errors)
        );
	}
}

void i210_debug_dump_tx_ring(struct i210_adapter *adapter)
{
	struct i210_tx_ring *ring = &adapter->tx_ring;
	void __iomem *hw = adapter->hw.hw_addr;

	pr_debug("TX ring: next_to_use=%u next_to_clean=%u TDH=%u TDT=%u\n",
        ring->next_to_use,
        ring->next_to_clean,
        readl(hw + I210_TDH(0)),
        readl(hw + I210_TDT(0))
    );
}