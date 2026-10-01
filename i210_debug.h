#ifndef _I210_DEBUG_H_
#define _I210_DEBUG_H_

#include "i210.h"

void i210_debug_log_tx(struct i210_adapter *adapter, struct sk_buff *skb);
void i210_debug_log_rx(struct i210_adapter *adapter, u16 index);
void i210_debug_dump_rx_ring(struct i210_adapter *adapter);
void i210_debug_dump_tx_ring(struct i210_adapter *adapter);
void i210_debug_dump_regs(struct i210_adapter *adapter);

#endif /* _I210_DEBUG_H_ */