#ifndef __LCM_I2C__H
#define __LCM_I2C__H

struct _lcm_i2c_dev {
        struct i2c_client *client;
};

int lcm_set_bias_volatage(unsigned char addr, unsigned char value);
#endif
