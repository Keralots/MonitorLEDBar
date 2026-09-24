# Wiring

## Parts

| Ref | Part | Notes |
|-----|------|-------|
| PD  | USB-C PD trigger board | set to 12 V; charger must offer a 12 V PDO (some only do 5/9 V) |
| U1  | Mini-360 buck module | 12 V in, trimmed to 5.0 V out |
| U2  | ESP32-C3 SuperMini | powered via its 5V pin |
| Q1  | IRLZ44N (TO-220) | low-side switch for the LED strip |
| R1  | 100 R | gate series resistor |
| R2  | 10 k | gate pull-down, keeps LEDs off during boot/flash |
| D1  | Schottky, e.g. 1N5819 / SS14 | buck 5V -> SuperMini 5V, blocks back-feed when USB is plugged in |
| C1  | 470 uF 25 V electrolytic (optional) | 12 V bulk cap near Q1 / strip |
| T1, T2 | TTP223 modules | default jumpers (momentary, active HIGH) |
| LED | 12 V single-color strip | |

## Diagram

Power:

```
 USB-C PD charger
        |
 [PD trigger, 12 V]
  VOUT+ ---+---------------+----------------------> 12V rail
           |               |
           |             [C1 470uF]
           |               |
  VOUT- ---|---------------+----------------------> GND rail
           |               |
     [Mini-360 IN+]  [Mini-360 IN-]
     [Mini-360 OUT+] [Mini-360 OUT-]
           |               |
         D1 (anode)        +--> GND rail
           |
         D1 (cathode) ----------> SuperMini 5V
                                  SuperMini GND <-- GND rail
```

LED switch:

```
 12V rail ------------------------------> LED strip +
                                          LED strip - --> Q1 DRAIN

 SuperMini GPIO10 --[R1 100R]--+--------> Q1 GATE
                               |
                           [R2 10k]
                               |
 GND rail ---------------------+--------> Q1 SOURCE
```

Touch pads:

```
 SuperMini 3V3 ---+-----------+
                  |           |
               T1 VCC      T2 VCC
 SuperMini GND ---+-----------+--> T1 GND, T2 GND
 SuperMini GPIO0 <---- T1 I/O   (dim / off)
 SuperMini GPIO1 <---- T2 I/O   (brighten / on)
```

Connection list (same as above, easier to follow while soldering):

| From | To |
|------|----|
| PD VOUT+ (12 V) | Mini-360 IN+, LED strip +, C1 + |
| PD VOUT- (GND)  | Mini-360 IN-, Q1 source, SuperMini GND, C1 - |
| Mini-360 OUT+ (5.0 V) | D1 anode |
| D1 cathode | SuperMini 5V |
| Mini-360 OUT- | GND |
| SuperMini 3V3 | T1 VCC, T2 VCC |
| SuperMini GND | T1 GND, T2 GND |
| T1 I/O | GPIO0 (dim / off) |
| T2 I/O | GPIO1 (brighten / on) |
| GPIO10 | R1 -> Q1 gate |
| Q1 gate | R2 -> Q1 source |
| Q1 drain | LED strip - |

IRLZ44N pinout (TO-220, text facing you, legs down): 1 = Gate, 2 = Drain,
3 = Source. The metal tab is Drain.

## Before first power-up

1. Power the Mini-360 alone from 12 V and trim it to 5.0 V with a meter
   **before** connecting the SuperMini. Modules often ship at a higher voltage.
2. Keep all grounds common (PD, buck, ESP, Q1 source, touch modules).
3. Size the 12 V and strip-minus wires for the strip's full current.
4. Do not touch the pads for about a second after power-up - TTP223 calibrates
   its baseline at power-on.

## Notes

- **Gate drive:** the IRLZ44N datasheet specifies RDS(on) down to VGS = 4.0 V
  (35 mOhm max at ID = 21 A) and VGS(th) = 1.0 - 2.0 V. The ESP drives only
  3.3 V, which is below the specified range. It works for a moderate current
  strip, but check Q1 temperature at full brightness. PWM runs at 5 kHz to keep
  switching loss low.
- **D1:** lets you plug USB in for flashing while 12 V is connected, without
  feeding USB 5 V back into the buck. If you skip D1, never connect USB while
  the 12 V supply is on. The ~0.3 - 0.45 V drop is fine for the SuperMini's
  3.3 V regulator.
- GPIO2, GPIO8 and GPIO9 are strapping pins (GPIO8 = onboard LED, GPIO9 = BOOT)
  and are left unused.
