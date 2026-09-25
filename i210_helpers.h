#ifndef _I210_HELPERS_H_
#define _I210_HELPERS_H_

#include "i210.h"
#include "i210_defines.h"

int i210_read_nvm_word(struct i210_adapter *adapter, u16 offset, u16 *data);

int i210_reset_hw(struct i210_adapter *adapter);
void i210_set_mac_address(struct i210_adapter *adapter, const u8 *mac);

void i210_configure_tx(struct i210_adapter *adapter);
int i210_setup_tx_resources(struct i210_adapter *adapter);
void i210_free_tx_resources(struct i210_adapter *adapter);
int i210_configure_tx_ring(struct i210_adapter *adapter);
void i210_enable_tx(struct i210_adapter *adapter);
bool i210_tx_ring_full(struct i210_tx_ring *tx_ring);
void i210_clean_tx(struct i210_adapter *adapter);

void i210_configure_rx(struct i210_adapter *adapter);
int i210_setup_rx_resources(struct i210_adapter *adapter);
int i210_configure_rx_ring(struct i210_adapter *adapter);
void i210_free_rx_resources(struct i210_adapter *adapter);
int i210_clean_rx(struct i210_adapter *adapter, int budget);
int i210_alloc_rx_buffer(struct i210_adapter *adapter, u16 i);

int i210_read_phy_reg(struct i210_adapter *adapter, u8 reg, u16 *value);
int i210_write_phy_reg(struct i210_adapter *adapter, u16 reg, u16 value);
int i210_dump_phy(struct i210_adapter *adapter);
int i210_get_phy_link(struct i210_adapter *adapter, bool *link);
int i210_phy_start_autoneg(struct i210_adapter *adapter);
int i210_phy_wait_link(struct i210_adapter *adapter, unsigned int timeout_ms);
int i210_init_phy(struct i210_adapter *adapter);

#endif /* _I210_HELPERS_H_ */