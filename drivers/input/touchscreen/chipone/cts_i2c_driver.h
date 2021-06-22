#ifndef __CTS_I2C_DRIVER__
#define __CTS_I2C_DRIVER__

#include "cts_core.h"

int cts_suspend(struct chipone_ts_data *cts_data);
int cts_resume(struct chipone_ts_data *cts_data);

#endif
