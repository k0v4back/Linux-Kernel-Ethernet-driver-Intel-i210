#ifndef _I210_HELPERS_H_
#define _I210_HELPERS_H_

#include "i210.h"
#include "i210_defines.h"

int i210_read_nvm_word(struct i210_adapter *adapter, u16 offset, u16 *data);

int i210_reset_hw(struct i210_adapter *adapter);
void i210_set_mac_address(struct i210_adapter *adapter, const u8 *mac);

void i210_configure_tx(struct i210_adapter *adapter);
void i210_configure_rx(struct i210_adapter *adapter);

int i210_setup_tx_resources(struct i210_adapter *adapter);
void i210_free_tx_resources(struct i210_adapter *adapter);
int i210_configure_tx_ring(struct i210_adapter *adapter);
void i210_enable_tx(struct i210_adapter *adapter);

bool i210_tx_ring_full(struct i210_tx_ring *tx_ring);

void i210_clean_tx(struct i210_adapter *adapter);

#endif /* _I210_HELPERS_H_ */