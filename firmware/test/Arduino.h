// Minimal Arduino API mock so the sketch can be compiled and simulated on a PC.
#pragma once
#include <stdint.h>
#include <math.h>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define A0 14
#define A1 15
#define F(s) (s)

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
void delayMicroseconds(unsigned int us);
uint32_t millis();

struct SerialMock {
  void begin(long) {}
  template <typename T> void print(T) {}
  template <typename T> void print(T, int) {}
  template <typename T> void println(T) {}
  template <typename T> void println(T, int) {}
};
extern SerialMock Serial;
