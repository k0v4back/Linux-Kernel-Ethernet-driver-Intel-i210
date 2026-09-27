#ifndef _I210_H_
#define _I210_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/ip.h>
#include <linux/if_arp.h>

struct i210_hw {
	void __iomem *hw_addr;
	void __iomem *flash_address;
};

struct i210_tx_desc {
	__le64 buffer_addr; 	/* Physical address of the data */
	__le32 cmd_type_len; 	/* Command + Type + Lenght */
	__le32 olinfo_status; 	/* offload-info + status */
};

struct i210_rx_desc {
	__le64 buffer_addr;
	__le16 length;
	__le16 checksum;
	u8 status;
	u8 errors;
	__le16 special;
};

struct i210_tx_buffer {
	struct sk_buff *skb;
	dma_addr_t dma;
	unsigned int len;
};

struct i210_rx_buffer {
	struct sk_buff *skb;
	dma_addr_t dma;
};

struct i210_tx_ring {
	struct i210_tx_desc *desc;
	dma_addr_t dma;

	struct i210_tx_buffer *buffer_info;

	unsigned int count;
	unsigned int size;

	u16 next_to_use;
	u16 next_to_clean;
};

struct i210_rx_ring {
	struct i210_rx_desc *desc;
	dma_addr_t dma;

	struct i210_rx_buffer *buffer_info;

	unsigned int count;
	unsigned int size;

	u16 next_to_clean;
};

struct i210_adapter {
	struct pci_dev *pdev;
	struct net_device *netdev;
	struct i210_hw hw;

	struct napi_struct napi;
	struct i210_tx_ring tx_ring;
	struct i210_rx_ring rx_ring;
};

#endif /* _I210_H_ */