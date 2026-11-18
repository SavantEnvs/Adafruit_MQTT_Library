// Fuzzes the client side of MQTT: every byte the broker sends us.
//
// Input: [0] packet-buffer size selector, [1] subscription payload-buffer size selector,
// [2..] the broker's byte stream. The harness drives the public API the way a sketch does
// (connect -> readSubscription x3 -> ping -> publish QoS1 -> unsubscribe -> processPackets ->
// disconnect), all reading from the one stream: CONNACK, SUBACK, PUBLISH, PUBACK, UNSUBACK,
// PINGRESP framing and the remaining-length decoding are all attacker-controlled.
#include "Adafruit_MQTT_Client.h"
#include "mock_client.h"

static volatile uint64_t sink;

static void on_uint32(uint32_t v) { sink += v; }
static void on_double(double d) { sink += (uint64_t)(d != d); }
static void on_buffer(char *s, uint16_t len) {
  for (uint16_t i = 0; i < len; i++)
    sink += (uint8_t)s[i];
  sink += (uint8_t)s[len]; // lastread is NUL-terminated past the reported length
}
static void on_buffer_large(char *s, size_t len) {
  for (size_t i = 0; i < len; i++)
    sink += (uint8_t)s[i];
  sink += (uint8_t)s[len];
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  // No smaller than the 29-byte CONNECT packet built from the fixed credentials below.
  static const size_t kBufSizes[] = {32, 48, 64, 150, 512, 1024, 4096, 128};
  static const size_t kSubSizes[] = {2, 8, 20, 64, 150, 512};
  if (size < 2)
    return -1;
  size_t bufsz = kBufSizes[data[0] & 7];
  size_t subsz = kSubSizes[data[1] % 6];
  data += 2;
  size -= 2;

  mayhem_clock_ms = 0;
  MockClient client(data, size);
  Adafruit_MQTT_Client mqtt(&client, "broker.example", 1883, "cid", "user",
                            "pass", bufsz);

  Adafruit_MQTT_Subscribe sa(&mqtt, "t/a", MQTT_QOS_0, subsz);
  Adafruit_MQTT_Subscribe sb(&mqtt, "t/b", MQTT_QOS_0, subsz);
  Adafruit_MQTT_Subscribe sc(&mqtt, "t/c", MQTT_QOS_1, subsz);
  Adafruit_MQTT_Subscribe sd(&mqtt, "t/d", MQTT_QOS_0, subsz);
  Adafruit_MQTT_Subscribe se(&mqtt, "t/e", MQTT_QOS_1, subsz);
  sa.setCallback(on_uint32);
  sb.setCallback(on_double);
  sc.setCallback(on_buffer);
  sd.setCallback(on_buffer_large);
  mqtt.subscribe(&sa);
  mqtt.subscribe(&sb);
  mqtt.subscribe(&sc);
  mqtt.subscribe(&sd);
  mqtt.subscribe(&se);

  mqtt.connect();
  for (int i = 0; i < 3; i++) {
    Adafruit_MQTT_Subscribe *s = mqtt.readSubscription(20);
    if (s)
      mqtt.processSubscriptionPacket(s);
  }
  mqtt.ping(1);
  mqtt.publish("t/a", "hi", 1);
  mqtt.unsubscribe(&se);
  mqtt.processPackets(20);
  mqtt.disconnect();
  return 0;
}
