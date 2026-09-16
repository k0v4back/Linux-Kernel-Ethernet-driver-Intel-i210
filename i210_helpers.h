#ifndef _I210_HELPERS_H_
#define _I210_HELPERS_H_

#include "i210.h"
#include "i210_defines.h"

static int i210_read_nvm_word(struct i210_adapter *adapter, u16 offset, u16 *data);

static int i210_reset_hw(struct i210_adapter *adapter);
static void i210_set_mac_address(struct i210_adapter *adapter, const u8 *mac);

static void i210_configure_tx(struct i210_adapter *adapter);
static void i210_configure_rx(struct i210_adapter *adapter);

#endif /* _I210_HELPERS_H_ */