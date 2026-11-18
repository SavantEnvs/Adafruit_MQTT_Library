// A Client whose "network" is a byte string: reads return the bytes in order,
// writes are captured. All state lives in the object, so each fuzz input starts clean.
#ifndef MAYHEM_MOCK_CLIENT_H
#define MAYHEM_MOCK_CLIENT_H

#include "Client.h"
#include <string>

class MockClient : public Client {
public:
  MockClient(const uint8_t *in, size_t n) : in_(in), n_(n) {}

  int connect(const char *, uint16_t) override {
    open_ = true;
    return 1;
  }
  size_t write(const uint8_t *buf, size_t size) override {
    sent_.append((const char *)buf, size);
    return size;
  }
  int available() override { return (int)(n_ - pos_); }
  int read() override { return pos_ < n_ ? in_[pos_++] : -1; }
  void stop() override { open_ = false; }
  uint8_t connected() override { return open_; }

  const std::string &sent() const { return sent_; }
  size_t consumed() const { return pos_; }

private:
  const uint8_t *in_;
  size_t n_;
  size_t pos_ = 0;
  bool open_ = true;
  std::string sent_;
};

#endif
