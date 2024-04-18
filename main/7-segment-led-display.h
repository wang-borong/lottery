#ifndef __7_SEGMENT_LED_DISPLAY_H
#define __7_SEGMENT_LED_DISPLAY_H

#define DIS_COMM_ANODE_MODE 1
#ifdef DIS_COMM_ANODE_MODE
typedef enum {
    DIS_0 = 0xC0,
    DIS_1 = 0xF9,
    DIS_2 = 0xA4,
    DIS_3 = 0xB0,
    DIS_4 = 0x99,
    DIS_5 = 0x92,
    DIS_6 = 0x82,
    DIS_7 = 0xF8,
    DIS_8 = 0x80,
    DIS_9 = 0x90,
    DIS_A = 0x88,
    DIS_B = 0x83,
    DIS_C = 0xC6,
    DIS_D = 0xA1,
    DIS_E = 0x86,
    DIS_F = 0x8E,
    DIS_Line = 0xBF,
    DIS_Blank = 0xFF,
    DIS_Dot = 0x7F,
    DIS_Full = 0x00
} dis_val_t;
#else
typedef enum {
    DIS_0 = 0x3F,
    DIS_1 = 0x06,
    DIS_2 = 0x5B,
    DIS_3 = 0x4F,
    DIS_4 = 0x66,
    DIS_5 = 0x6D,
    DIS_6 = 0x7D,
    DIS_7 = 0x07,
    DIS_8 = 0x7F,
    DIS_9 = 0x6F,
    DIS_A = 0x77,
    DIS_B = 0x7C,
    DIS_C = 0x39,
    DIS_D = 0x5E,
    DIS_E = 0x79,
    DIS_F = 0x71,
    DIS_Line = 0x40,
    DIS_Blank = 0x00,
    DIS_Dot = 0x80,
    DIS_Full = 0xFF
} dis_val_t;
#endif

extern dis_val_t dis_val[4];

void display_init();
void display_deinit();

#endif
