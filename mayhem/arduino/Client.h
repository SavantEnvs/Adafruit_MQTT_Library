// Arduino `Client` interface (the abstract network client).
#ifndef MAYHEM_CLIENT_H
#define MAYHEM_CLIENT_H

#include "Arduino.h"

class Client {
public:
  virtual ~Client() {}
  virtual int connect(const char *host, uint16_t port) = 0;
  virtual size_t write(const uint8_t *buf, size_t size) = 0;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual void stop() = 0;
  virtual uint8_t connected() = 0;
};

#endif
