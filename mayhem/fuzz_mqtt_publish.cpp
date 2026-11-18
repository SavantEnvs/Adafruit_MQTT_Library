// Fuzzes the publish side: packet construction for caller-supplied topic and payload, in packet
// buffers of many sizes (the area of the "protect against memory corruption" and packet-length
// fixes).
//
// Input: [0] packet-buffer size selector, [1] bits0-1 QoS, bit2 retain, bits3-5 publish
// variant, [2] topic length, then the topic bytes, then the payload.
// Invariant checked after every publish (it sends one packet): the packet's own remaining-length field
// matches the bytes actually put on the wire.
#include "Adafruit_MQTT_Client.h"
#include "mock_client.h"
#include <string>

static void check_wire(const std::string &sent) {
  if (sent.empty())
    return;
  size_t value = 0, mult = 1, i = 1;
  uint8_t b;
  do {
    if (i >= sent.size() || i > 4)
      abort();
    b = (uint8_t)sent[i++];
    value += (b & 0x7F) * mult;
    mult *= 128;
  } while (b & 0x80);
  if (i + value != sent.size())
    abort();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  static const size_t kBufSizes[] = {8, 16, 24, 32, 64, 150, 512, 4096};
  if (size < 3)
    return -1;
  size_t bufsz = kBufSizes[data[0] & 7];
  uint8_t qos = data[1] & 3;
  bool retain = (data[1] >> 2) & 1;
  unsigned variant = (data[1] >> 3) & 7;
  size_t topiclen = data[2];
  data += 3;
  size -= 3;
  if (topiclen > size)
    topiclen = size;
  std::string topic((const char *)data, topiclen);
  std::string payload((const char *)data + topiclen, size - topiclen);

  mayhem_clock_ms = 0;
  // Pre-load the PUBACK for the first packet id so QoS 1 publishes can complete.
  static const uint8_t kPuback[] = {0x40, 0x02, 0x00, 0x01};
  MockClient client(kPuback, sizeof kPuback);
  Adafruit_MQTT_Client mqtt(&client, "broker.example", 1883, "cid", "user",
                            "pass", bufsz);
  Adafruit_MQTT_Publish pub(&mqtt, topic.c_str(), qos);

  switch (variant) {
  case 0:
    mqtt.publish(topic.c_str(), (uint8_t *)&payload[0], payload.size(), qos,
                 retain);
    break;
  case 1:
    mqtt.publish(topic.c_str(), payload.c_str(), qos, retain);
    break;
  case 2: {
    int32_t v = 0;
    memcpy(&v, payload.data(), payload.size() < 4 ? payload.size() : 4);
    pub.publish(v, retain);
    break;
  }
  case 3: {
    uint32_t v = 0;
    memcpy(&v, payload.data(), payload.size() < 4 ? payload.size() : 4);
    pub.publish(v, retain);
    break;
  }
  case 4: {
    // A 4-byte float, as `double` is on AVR boards.
    float f = 0;
    memcpy(&f, payload.data(), payload.size() < 4 ? payload.size() : 4);
    pub.publish((double)f, 2, retain);
    break;
  }
  default:
    pub.publish((uint8_t *)&payload[0], payload.size(), retain);
    break;
  }
  check_wire(client.sent());
  return 0;
}
