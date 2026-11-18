// Minimal host-side Arduino core: just what Adafruit_MQTT.{h,cpp} use.
#ifndef MAYHEM_ARDUINO_H
#define MAYHEM_ARDUINO_H

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef bool boolean;
typedef uint8_t byte;

#define HEX 16
#define DEC 10

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define PSTR(s) (s)
#define PROGMEM
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))

// Deterministic virtual clock: delay() advances it, and every millis() call
// ticks it, so loops that wait on elapsed time terminate even when the
// connection is closed and delay() is never reached.
extern uint32_t mayhem_clock_ms;
inline uint32_t millis() { return mayhem_clock_ms++; }
inline void delay(uint32_t ms) { mayhem_clock_ms += ms; }

// Serial output is discarded.
struct MayhemSerial {
  template <class T> size_t print(T) { return 0; }
  template <class T> size_t print(T, int) { return 0; }
  template <class T> size_t println(T) { return 0; }
  template <class T> size_t println(T, int) { return 0; }
  size_t println() { return 0; }
  size_t write(uint8_t) { return 1; }
};
extern MayhemSerial Serial;

inline char *ltoa(long v, char *buf, int radix) {
  (void)radix;
  sprintf(buf, "%ld", v);
  return buf;
}
inline char *ultoa(unsigned long v, char *buf, int radix) {
  (void)radix;
  sprintf(buf, "%lu", v);
  return buf;
}
// Same contract as the Arduino AVR/ESP cores: writes the whole formatted
// number to the caller's buffer.
inline char *dtostrf(double val, signed char width, unsigned char prec,
                     char *sout) {
  sprintf(sout, "%*.*f", (int)width, (int)prec, val);
  return sout;
}

#endif
