#include "Arduino.h"

#include "max7219.h"

MAX7219::MAX7219(uint8_t num_display, int datain, int cs, int clk) {
  _num_display = num_display;
  scr = new byte [_num_display * 8 + 8]; // +8 for scrolled char
  // pin
  _datain = datain;
  _cs = cs;
  _clk = clk;
}

MAX7219::~MAX7219() {
  delete(scr);
}

void MAX7219::sendCmd(int addr, byte cmd, byte data) {
  digitalWrite(_cs, LOW);
  for (int i = _num_display - 1; i >= 0; i--) {
    shiftOut(_datain, _clk, MSBFIRST, i == addr ? cmd : 0);
    shiftOut(_datain, _clk, MSBFIRST, i == addr ? data : 0);
  }
  digitalWrite(_cs, HIGH);
}

void MAX7219::sendCmdAll(byte cmd, byte data) {
  digitalWrite(_cs, LOW);
  for (int i = _num_display - 1; i >= 0; i--) {
    shiftOut(_datain, _clk, MSBFIRST, cmd);
    shiftOut(_datain, _clk, MSBFIRST, data);
  }
  digitalWrite(_cs, HIGH);
}

void MAX7219::refresh(int addr) {
  for (int i = 0; i < 8; i++)
    this->sendCmd(addr, i + CMD_DIGIT0, scr[addr * 8 + i]);
}

void MAX7219::refreshAllRot270() {
  byte mask = 0x01;
  for (int c = 0; c < 8; c++) {
    digitalWrite(_cs, LOW);
    for (int i = _num_display - 1; i >= 0; i--) {
      byte bt = 0;
      for (int b = 0; b < 8; b++) {
        bt <<= 1;
        if (scr[i * 8 + b] & mask) bt |= 0x01;
      }
      shiftOut(_datain, _clk, MSBFIRST, CMD_DIGIT0 + c);
      shiftOut(_datain, _clk, MSBFIRST, bt);
    }
    digitalWrite(_cs, HIGH);
    mask <<= 1;
  }
}

void MAX7219::refreshAllRot90() {
  byte mask = 0x80;
  for (int c = 0; c < 8; c++) {
    digitalWrite(_cs, LOW);
    for (int i = _num_display - 1; i >= 0; i--) {
      byte bt = 0;
      for (int b = 0; b < 8; b++) {
        bt >>= 1;
        if (scr[i * 8 + b] & mask) bt |= 0x80;
      }
      shiftOut(_datain, _clk, MSBFIRST, CMD_DIGIT0 + c);
      shiftOut(_datain, _clk, MSBFIRST, bt);
    }
    digitalWrite(_cs, HIGH);
    mask >>= 1;
  }
}

void MAX7219::refreshAll() {
#if ROTATE==270
  refreshAllRot270();
#elif ROTATE==90
  refreshAllRot90();
#else
  for (int c = 0; c < 8; c++) {
    digitalWrite(_cs, LOW);
    for (int i = _num_display - 1; i >= 0; i--) {
      shiftOut(_datain, _clk, MSBFIRST, CMD_DIGIT0 + c);
      shiftOut(_datain, _clk, MSBFIRST, scr[i * 8 + c]);
    }
    digitalWrite(_cs, HIGH);
  }
#endif
}

void MAX7219::clr() {
  for (int i = 0; i < _num_display * 8; i++) scr[i] = 0;
}

void MAX7219::scrollLeft() {
  for (int i = 0; i < _num_display * 8 + 7; i++) scr[i] = scr[i + 1];
}

void MAX7219::invert() {
  for (int i = 0; i < _num_display * 8; i++) scr[i] = ~scr[i];
}

void MAX7219::initMAX7219() {
  pinMode(_datain, OUTPUT);
  pinMode(_clk, OUTPUT);
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);
  this->sendCmdAll(CMD_DISPLAYTEST, 0);
  this->sendCmdAll(CMD_SCANLIMIT, 7);
  this->sendCmdAll(CMD_DECODEMODE, 0);
  this->sendCmdAll(CMD_INTENSITY, 1);
  this->sendCmdAll(CMD_SHUTDOWN, 0);
  this->clr();
  this->refreshAll();
}

int MAX7219::size_of_scr(){
  return sizeof(scr);
  }

byte *MAX7219::get_scr(){
  return scr;
  }

void MAX7219::showDigit(char ch, int col, const uint8_t *data) {
  if (delta_y < -8 | delta_y > 8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += delta_x;
  for (int i = 0; i < w; i++)
    if (col + i >= 0 && col + i < _num_display * 8) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if (!delta_y)
        scr[col + i] = v; else scr[col + i] |= delta_y > 0 ? v >> delta_y : v << -delta_y;
    }
}

int MAX7219::showChar(char ch, int col, const uint8_t *data) {
  int len = pgm_read_byte(data);
  int i, w = pgm_read_byte(data + 1 + ch * len);
  if (delta_y < -8 | delta_y > 8) return w;
  col += delta_x;
  for (i = 0; i < w; i++)
    if (col + i >= 0 && col + i < _num_display * 8) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if (!delta_y) scr[col + i] = v; else scr[col + i] |= delta_y > 0 ? v >> delta_y : v << -delta_y;
    }
  return w;
}

void MAX7219::showString(int x, char *s) {
  while (*s) {
    unsigned char c = *s++;
    if (c < ' ' || c > '~' + 22) continue;
    c -= 32;
    int w = showChar(c, x, font);
    x += w + 1;
  }
}

void MAX7219::scrollChar(unsigned char c, int del) {
  if (c < ' ' || c > '~' + 22) return;
  c -= 32;
  int w = _printChar(c, font);
  for (int i = 0; i < w + 1; i++) {
    delay(del);
    this->scrollLeft();
    this->refreshAll();
  }
}

void MAX7219::scrollString(char *s, int del) {
  while (*s) scrollChar(*s++, del);
}

void MAX7219::setColumn(int col, byte v) {
  if (delta_y < -8 | delta_y > 8) return;
  col += delta_x;
  if (col >= 0 && col < 32)
      if (!delta_y) scr[col] = v; else scr[col] |= delta_y > 0 ? v >> delta_y : v << -delta_y;
}


// private
int MAX7219::_printChar(unsigned char ch, const uint8_t *data) {
  int len = pgm_read_byte(data);
  int i, w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[_num_display * 8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[_num_display * 8 + i] = 0;
  return w;
}
