#ifndef __MAX7219_H__
#define __MAX7219_H__

#include "Arduino.h"
#include "fonts.h"

// MAX7219 commands:
#define CMD_NOOP   0
#define CMD_DIGIT0 1
#define CMD_DIGIT1 2
#define CMD_DIGIT2 3
#define CMD_DIGIT3 4
#define CMD_DIGIT4 5
#define CMD_DIGIT5 6
#define CMD_DIGIT6 7
#define CMD_DIGIT7 8
#define CMD_DECODEMODE  9
#define CMD_INTENSITY   10
#define CMD_SCANLIMIT   11
#define CMD_SHUTDOWN    12
#define CMD_DISPLAYTEST 15

#define MAX_INTENSITY 0xf

#define ROTATE 90

class MAX7219 {
  public:
    int delta_x = 0, delta_y = 0;

    MAX7219(uint8_t num_display = 4, int datain = 15 , int cs = 13, int clk = 12);
    ~MAX7219();

    uint8_t get_num_disp();
    void sendCmd(int addr, byte cmd, byte data);
    void sendCmdAll(byte cmd, byte data);
    void refresh(int addr);
    void refreshAllRot270();
    void refreshAllRot90();
    void refreshAll();
    void clr();
    void scrollLeft();
    void invert();
    void initMAX7219();

    int size_of_scr();
    byte *get_scr();
    
    int showChar(char ch, int col, const uint8_t *data);
    void showDigit(char ch, int col, const uint8_t *data);
    void showString(int x, char *s);
    void scrollChar(unsigned char c, int del);
    void scrollString(char *s, int del);
    void setColumn(int col, byte v);


  private:
    uint8_t _num_display;
    byte *scr;
    // pin
    int _datain = 15 , _cs = 13, _clk = 12;

    int _printChar(unsigned char ch, const uint8_t *data);

};

#endif
