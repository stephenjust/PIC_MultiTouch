
#include <Compiler.h>
#include <touch_comm.h>
#include "i2c.h"
#include "delays.h"

#define I2C_SLAVE 0x38

touch_data t_data;

#define LED2On() LATCbits.LATC6 = 1
#define LED2Off() LATCbits.LATC6 = 0
#define LED3On() LATCbits.LATC3 = 1
#define LED3Off() LATCbits.LATC3 = 0

/**
 * Return the number of touch points
 * @return
 */
unsigned int touch_points(void) {
    unsigned int num_points;

    // Read number of points
    i2c_Start();
    i2c_Address(I2C_SLAVE, I2C_WRITE);
    i2c_Write(0x02);
    i2c_Restart();
    i2c_Address(I2C_SLAVE, I2C_READ);
    num_points = i2c_Read(0);
    i2c_Stop();

    num_points = num_points & 0b00000111;

    return num_points;
}

void touch_read(void) {
    unsigned int i;

    // Read one byte
    i2c_Start();      			// send Start
    i2c_Address(I2C_SLAVE, I2C_WRITE);	// Send slave address with write operation
    i2c_Write(0x00);			// Set register to start reading
    i2c_Restart();			// Restart
    i2c_Address(I2C_SLAVE, I2C_READ);	// Send slave address with read operation

    for (i = 0; i < 0x1E; i++) {
        t_data.raw[i] = i2c_Read(1);
    }
    t_data.raw[0x1E] = i2c_Read(0);

   i2c_Stop();				// send Stop
}