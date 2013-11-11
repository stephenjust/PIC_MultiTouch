/* 
 * File:   touch_comm.h
 * Author: Stephen
 *
 * Created on August 3, 2013, 5:36 PM
 */

#ifndef TOUCH_COMM_H
#define	TOUCH_COMM_H

typedef union {
    unsigned int raw[31];
    struct {
        unsigned int DEVICE_MODE;
        unsigned int GEST_ID;
        unsigned int TD_STATUS;
        unsigned int TOUCH1_XH;
        unsigned int TOUCH1_XL;
        unsigned int TOUCH1_YH;
        unsigned int TOUCH1_YL;
        char pad1[2];
        unsigned int TOUCH2_XH;
        unsigned int TOUCH2_XL;
        unsigned int TOUCH2_YH;
        unsigned int TOUCH2_YL;
        char pad2[2];
        unsigned int TOUCH3_XH;
        unsigned int TOUCH3_XL;
        unsigned int TOUCH3_YH;
        unsigned int TOUCH3_YL;
        char pad3[2];
        unsigned int TOUCH4_XH;
        unsigned int TOUCH4_XL;
        unsigned int TOUCH4_YH;
        unsigned int TOUCH4_YL;
        char pad4[2];
        unsigned int TOUCH5_XH;
        unsigned int TOUCH5_XL;
        unsigned int TOUCH5_YH;
        unsigned int TOUCH5_YL;
    } data;
} touch_data;

extern touch_data t_data;

#ifdef	__cplusplus
extern "C" {
#endif

    void touch_read(void);
    void touch_send(void);
    unsigned int touch_points(void);


#ifdef	__cplusplus
}
#endif

#endif	/* TOUCH_COMM_H */

