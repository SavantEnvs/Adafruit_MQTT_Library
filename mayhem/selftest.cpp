// Known-answer tests for the library's wire format, run with the project's normal flags.
// Prints one "ok <name>" / "FAIL <name>" line per check and "TOTAL <n>" last.
#include "Adafruit_MQTT_Client.h"
#include "mock_client.h"
#include <string>

static int total = 0, failed = 0;
static void check(const char *name, bool cond) {
  total++;
  if (cond) {
    printf("ok %s\n", name);
  } else {
    failed++;
    printf("FAIL %s\n", name);
  }
}
static std::string hex(const std::string &s) {
  std::string out;
  char b[4];
  for (unsigned char c : s) {
    snprintf(b, sizeof b, "%02x", c);
    out += b;
  }
  return out;
}
static std::string bytes(std::initializer_list<int> l) {
  std::string s;
  for (int v : l)
    s.push_back((char)v);
  return s;
}

static std::string got_payload;
static uint32_t got_uint = 0;
static void cb_buf(char *s, uint16_t len) { got_payload.assign(s, len); }
static void cb_u32(uint32_t v) { got_uint = v; }

struct Rig {
  std::string in;
  MockClient client;
  Adafruit_MQTT_Client mqtt;
  Rig(const std::string &stream, size_t bufsz = 150)
      : in(stream), client((const uint8_t *)in.data(), in.size()),
        mqtt(&client, "broker", 1883, "cid", "user", "pass", bufsz) {
    mayhem_clock_ms = 0;
  }
};

int main() {
  {
    Rig r("");
    bool ok = r.mqtt.publish("feeds/temp", "hello", 0, false);
    check("publish qos0 returns true", ok);
    check("publish qos0 wire bytes",
          hex(r.client.sent()) == hex(bytes({0x30, 0x11, 0x00, 0x0a}) +
                                      "feeds/temp" + "hello"));
  }
  {
    Rig r(bytes({0x40, 0x02, 0x00, 0x01}));
    bool ok = r.mqtt.publish("a/b", "xyz", 1, true);
    check("publish qos1 retain acked", ok);
    check("publish qos1 retain wire bytes",
          hex(r.client.sent()) ==
              hex(bytes({0x33, 0x0a, 0x00, 0x03}) + "a/b" +
                  bytes({0x00, 0x01}) + "xyz"));
  }
  {
    Rig r(bytes({0x40, 0x02, 0x00, 0x09}));
    check("publish qos1 wrong packet id rejected",
          !r.mqtt.publish("a/b", "xyz", 1, false));
  }
  {
    Rig r("", 16);
    r.mqtt.publish("t", "0123456789ABCDEFGHIJ", 0, false);
    check("publish truncates payload to the packet buffer",
          hex(r.client.sent()) ==
              hex(bytes({0x30, 0x0e, 0x00, 0x01}) + "t" + "0123456789A"));
  }
  {
    Rig r("");
    std::string big(200, 'x');
    r.mqtt.publish("t", (uint8_t *)big.data(), big.size(), 0, false);
    check("publish with 200-byte payload truncated to 150-byte buffer",
          r.client.sent().size() == 150 && (uint8_t)r.client.sent()[0] == 0x30 &&
              (uint8_t)r.client.sent()[1] == 0x93 &&
              (uint8_t)r.client.sent()[2] == 0x01);
  }
  {
    Rig r("", 1024);
    std::string big(300, 'y');
    r.mqtt.publish("t", (uint8_t *)big.data(), big.size(), 0, false);
    check("publish with 300-byte payload uses 2-byte remaining length",
          r.client.sent().size() == 1 + 2 + 3 + 300 &&
              hex(r.client.sent().substr(0, 6)) == "30" "af02" "0001" "74");
  }
  {
    Rig r(bytes({0x20, 0x02, 0x00, 0x00}));
    int8_t rc = r.mqtt.connect();
    check("connect accepts CONNACK 0", rc == 0);
    check("connect packet wire bytes",
          hex(r.client.sent()) ==
              hex(bytes({0x10, 0x1b, 0x00, 0x04}) + "MQTT" +
                  bytes({0x04, 0xc2, 0x01, 0x2c, 0x00, 0x03}) + "cid" +
                  bytes({0x00, 0x04}) + "user" + bytes({0x00, 0x04}) + "pass"));
  }
  {
    Rig r(bytes({0x20, 0x02, 0x00, 0x05}));
    check("connect reports refused code 5", r.mqtt.connect() == 5);
  }
  {
    Rig r(bytes({0x20, 0x02, 0x00}));
    check("connect fails on truncated CONNACK", r.mqtt.connect() == -1);
  }
  {
    Rig r(bytes({0xd0, 0x00}));
    check("ping answered by PINGRESP", r.mqtt.ping(1));
    check("ping wire bytes", hex(r.client.sent()) == "c000");
  }
  {
    Rig r("");
    check("ping without reply fails", !r.mqtt.ping(1));
  }
  {
    std::string pkt = bytes({0x30, 0x09, 0x00, 0x03}) + "t/a" + "42" + "!!";
    Rig r(pkt);
    Adafruit_MQTT_Subscribe sub(&r.mqtt, "t/a", MQTT_QOS_0);
    sub.setCallback(cb_u32);
    r.mqtt.subscribe(&sub);
    got_uint = 0;
    Adafruit_MQTT_Subscribe *s = r.mqtt.readSubscription(100);
    check("readSubscription matches topic", s == &sub);
    if (s)
      r.mqtt.processSubscriptionPacket(s);
    check("uint32 callback parses payload", got_uint == 42);
    check("subscription datalen", sub.datalen == 4);
  }
  {
    std::string pkt = bytes({0x30, 0x0b, 0x00, 0x03}) + "T/A" + "hello!";
    Rig r(pkt);
    Adafruit_MQTT_Subscribe sub(&r.mqtt, "t/a");
    sub.setCallback(cb_buf);
    r.mqtt.subscribe(&sub);
    got_payload.clear();
    r.mqtt.processPackets(100);
    check("buffer callback receives payload, topic match is case-insensitive",
          got_payload == "hello!");
  }
  {
    std::string pkt = bytes({0x32, 0x0c, 0x00, 0x03}) + "t/a" +
                      bytes({0x12, 0x34}) + "data!";
    Rig r(pkt);
    Adafruit_MQTT_Subscribe sub(&r.mqtt, "t/a", MQTT_QOS_1);
    sub.setCallback(cb_buf);
    r.mqtt.subscribe(&sub);
    got_payload.clear();
    r.mqtt.processPackets(100);
    check("qos1 payload skips packet id", got_payload == "data!");
    check("qos1 publish is acknowledged with PUBACK",
          hex(r.client.sent()) == "40021234");
  }
  {
    std::string pkt = bytes({0x30, 0x0f, 0x00, 0x03}) + "t/a" + "ABCDEFGHIJ";
    Rig r(pkt);
    Adafruit_MQTT_Subscribe sub(&r.mqtt, "t/a", MQTT_QOS_0, 6);
    sub.setCallback(cb_buf);
    r.mqtt.subscribe(&sub);
    got_payload.clear();
    r.mqtt.processPackets(100);
    check("payload larger than subscription buffer is truncated",
          got_payload == "ABCDE");
  }
  {
    std::string pkt = bytes({0x30, 0x30, 0x00, 0x03}) + "t/a" +
                      std::string(0x2b, 'z');
    Rig r(pkt, 16);
    Adafruit_MQTT_Subscribe sub(&r.mqtt, "t/a");
    sub.setCallback(cb_buf);
    r.mqtt.subscribe(&sub);
    got_payload = "untouched";
    r.mqtt.processPackets(100);
    check("packet larger than the packet buffer is dropped",
          got_payload == "untouched");
  }
  {
    std::string pkt = bytes({0x30, 0xff, 0xff, 0xff, 0xff, 0x7f}) + "t/a";
    Rig r(pkt);
    Adafruit_MQTT_Subscribe sub(&r.mqtt, "t/a");
    sub.setCallback(cb_buf);
    r.mqtt.subscribe(&sub);
    got_payload = "untouched";
    r.mqtt.processPackets(100);
    check("5-byte remaining length is rejected as malformed",
          got_payload == "untouched" && !r.client.connected());
  }
  printf("TOTAL %d\n", total);
  return failed ? 1 : 0;
}
