#include "pins.h"

static const PinInfo PINS[] = {
  {0, true, ""},
  {1, true, ""},
  {2, false, "strapping pin, a connected module can stop the board from booting"},
  {3, true, ""},
  {4, true, "JTAG, free when debugging over USB"},
  {5, true, "JTAG, free when debugging over USB"},
  {6, true, "JTAG, free when debugging over USB"},
  {7, true, "JTAG, free when debugging over USB"},
  {8, false, "strapping pin, onboard LED"},
  {9, false, "strapping pin, BOOT button"},
  {10, true, ""},
  {20, true, "UART0 RX"},
  {21, false, "UART0 TX, driven by the boot ROM log at startup"},
};

bool pinUsable(uint8_t gpio) {
  for (const auto &p : PINS)
    if (p.gpio == gpio) return p.usable;
  return false;
}

void pinsFillJson(JsonArray arr) {
  for (const auto &p : PINS) {
    JsonObject o = arr.add<JsonObject>();
    o["gpio"] = p.gpio;
    o["usable"] = p.usable;
    o["note"] = p.note;
  }
}
