
#include <Compiler.h>
#include <touch_comm.h>
#include "i2c.h"

#define I2C_SLAVE 0x70

touch_data t_data;

void touch_read(void) {
    unsigned int i;

    // Read one byte (i.e. servo pos of one servo)
    i2c_Start();      			// send Start
    i2c_Address(I2C_SLAVE, I2C_WRITE);	// Send slave address with write operation
    i2c_Write(0x00);			// Set register for servo 0
    i2c_Restart();			// Restart
    i2c_Address(I2C_SLAVE, I2C_READ);	// Send slave address with read operation

    for (i = 0; i < 0x1E; i++) {
        t_data.raw[i] = i2c_Read(1);
    }
    t_data.raw[0x1E] = i2c_Read(0);

   i2c_Stop();				// send Stop
}