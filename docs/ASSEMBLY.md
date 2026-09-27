# Assembly guide

How to put the printed MonitorLEDBar together. Each step has a CAD render
first and then photos of a real build. The renders show bought parts (screws,
nuts, AA cells, TTP223 boards, LED strip, cables) as simple stand-ins, and most
of them use the short 60 mm test arms so the details stay visible.

**Print files:** [MakerWorld](https://makerworld.com/models/3362536).

Screw variant of the housing (`02_ShallowHousing_sruby`): every M3 screw cuts
its own thread in the plastic, no heat-set inserts. Drive each screw by hand
the first time; when you put a screw back later, turn it backwards until it
clicks into the existing thread, then tighten.

## Contents

- [What you need](#what-you-need)
- [1. Solder the touch modules](#1-solder-the-touch-modules)
- [2. Thread the harness through the central mount](#2-thread-the-harness-through-the-central-mount)
- [3. Touch modules into the head](#3-touch-modules-into-the-head)
- [4. Arms](#4-arms)
- [5. LED strip](#5-led-strip)
- [6. Diffusers](#6-diffusers)
- [7. End caps](#7-end-caps)
- [8. Batteries and electronics](#8-batteries-and-electronics)
- [9. Nuts into the nut bar](#9-nuts-into-the-nut-bar)
- [10. Yoke on the rail](#10-yoke-on-the-rail)
- [11. Cover](#11-cover)
- [12. Lamp onto the housing](#12-lamp-onto-the-housing)
- [13. On the monitor](#13-on-the-monitor)

## What you need

![All printed parts](assembly/renders/00_overview.png)

![Arms on the print bed](assembly/photos/00_print_0.jpg)
![The mount parts](assembly/photos/00_print_1.jpg)
![The diffusers](assembly/photos/00_print_2.jpg)

| # | Part | Qty | Colour |
|---|---|---|---|
| 01 | CentralMount | 1 | body |
| 02 | ShallowHousing (screw variant) | 1 | body |
| 03 | Cover | 1 | body |
| 04 | SliderYoke | 1 | body |
| 05 | NutBar | 1 | body |
| 06 / 07 | Arm left / right | 1 + 1 | body |
| 08 / 09 | Diffuser right / left | 1 + 1 | white |
| 10 / 11 | EndCap left / right | 1 + 1 | white |

Hardware:

- 4 x M3x10 cylinder- or pan-head screw (cover; M3x12 at most)
- 2 x M3x25 screw (yoke)
- 2 x M3x12 screw + 2 x M3 washer + 2 x M3 nut (5.5 mm across flats) for the adjustment
- 4 x AA cell (ballast; used-up cells are fine)
- LED strip up to 11 mm wide, two TTP223 touch modules, a flat two-core LED cable,
  four thin leads for the touch modules
- electronics: see [Hardware](../README.md#hardware) in the README, plus a
  small perfboard and a 4-pin connector
- USB-C cable, one zip tie
- optional: 1 mm self-adhesive silicone or felt pads (front hook, two rear contact pads)

## 1. Solder the touch modules

![Two TTP223 modules, shared VCC and GND, one I/O wire each](assembly/photos/01_0.jpg)

Solder the two TTP223 modules together: VCC to VCC and GND to GND with short
jumpers, then one lead each from the shared VCC and GND and one signal (I/O)
lead from each module: four thin wires in all. Leave the wires long enough to
reach the housing.

## 2. Thread the harness through the central mount

![Thread the harness through the central mount](assembly/renders/01_harness.png)

Feed the four thin wires and the flat two-core LED cable through the central
mount (01) before anything else goes in. From the arm sockets they go up over
the socket wall and through the window in the back of the head; pull a loop
out there, then push the wires down the shaft. Under the plate the groove is
open from below: pick them up with tweezers or a finger, lay them back along
the groove and tuck them under the small bridge at the end of the plate. From
there they run under the rail towards the housing.

| | |
|---|---|
| ![A wire from the head out under the plate](assembly/photos/02_0.jpg) | ![The loop at the window in the back of the head](assembly/photos/02_1.jpg) |
| A wire from the head out under the plate | The loop at the window in the back of the head |
| ![The window from behind](assembly/photos/02_2.jpg) | ![The harness in the groove under the plate](assembly/photos/02_3.jpg) |
| The window from behind | The harness in the groove under the plate |

## 3. Touch modules into the head

![Touch modules into the head](assembly/renders/02_ttp223.png)

Push the two TTP223 modules into their pockets in the head, from the arm
sockets inwards, sensor face towards the front.

![A module in its pocket, seen through the arm socket](assembly/photos/03_0.jpg)

## 4. Arms

![Arms](assembly/renders/03_arms.png)

Slide the left (06) and right (07) arm into the mount until they stop. The arm
ends close the pockets and hold the touch modules. The LED cable comes out
through the 3 mm gap between the two arm ends.

| | |
|---|---|
| ![An arm in the mount](assembly/photos/04_0.jpg) | ![The LED cable between the arm ends](assembly/photos/04_1.jpg) |
| An arm in the mount | The LED cable between the arm ends |

## 5. LED strip

![LED strip](assembly/renders/04_led_strip.png)

Stick the strip on the roof of the channel under the arms, one piece across
the centre, and solder the LED cable to the strip's pads in the gap between
the arm ends. **Stop 7 mm short of each arm's end**: the end cap's plug goes
there. The strip shelf is 11 mm wide.

![The strip across the centre, cable soldered to its pads](assembly/photos/05_0.jpg)

## 6. Diffusers

![Diffusers](assembly/renders/05_diffusers.png)

Slide both diffusers (08, 09) in from the arm ends, nose first, visible face
down. In the middle the two noses pass each other side by side over 4 mm.

![The two diffusers meeting under the mount](assembly/photos/06_0.jpg)

## 7. End caps

![End caps](assembly/renders/06_end_caps.png)

Press the end caps (10, 11) on. The end of the diffuser tucks 2.0 mm under the
cap's white skin.

![An end cap on](assembly/photos/07_0.jpg)

## 8. Batteries and electronics

![Batteries and electronics](assembly/renders/07_cells_cable.png)

Lay the 4 AA cells in the two bays at the back of the housing (02), two per
side, one on the other. The electronics sit above the cells or in the middle;
plug the harness from the mount into your board. The USB-C cable leaves
through the 13 x 8 mm window at the bottom of the back wall; hold it with a
zip tie through the two holes in the floor.

The electronics are an ESP32-C3 SuperMini, an IRLZ44N MOSFET switching the
12 V LED strip and a Mini-360 step-down converter feeding the ESP32, on a small
perfboard with a 4-pin connector for the touch-module harness. Wire them as in
the diagram below; the full pin list and a power-up checklist are in
[WIRING.md](WIRING.md).

![Wiring diagram](wiring.png)

Power: 12 V from a USB-C PD trigger board set to 12 V (the charger must offer a
12 V PD profile); a plain 12 V supply works as well. Set the Mini-360 to
5.0 V **before** you connect the ESP32. Flash the firmware as described in
[Building and flashing](../README.md#building-and-flashing).

| | |
|---|---|
| ![ESP32-C3 SuperMini, IRLZ44N and the Mini-360 converter](assembly/photos/08_0.jpg) | ![The ESP32-C3 wiring](assembly/photos/08_1.jpg) |
| ESP32-C3 SuperMini, IRLZ44N and the Mini-360 converter | The ESP32-C3 wiring |
| ![AA cells in the bays](assembly/photos/08_2.jpg) | ![The board on top of the cells](assembly/photos/08_3.jpg) |
| AA cells in the bays | The board on top of the cells |
| ![Harness plugged into the board](assembly/photos/08_4.jpg) | ![First light before closing](assembly/photos/08_5.jpg) |
| Harness plugged into the board | First light before closing |

Light it up once before you close the housing.

## 9. Nuts into the nut bar

![Nuts into the nut bar](assembly/renders/08_nuts.png)

Press the two M3 nuts into the pockets of the nut bar (05). They are a press
fit (5.55 mm pocket for a 5.5 mm nut): push with a flat screwdriver or a vice.
If too tight, warm the nut with a soldering iron and press it in.

![Both nuts in](assembly/photos/09_0.jpg)

## 10. Yoke on the rail

![Yoke on the rail](assembly/renders/09_yoke_on_rail.png)

Put the yoke (04) over the rail of the central mount and the nut bar under it,
pockets up. Drive the two M3x12 screws, each with a washer, from the top
through the yoke and the rail slot into the nuts. Leave them loose for now.

| | |
|---|---|
| ![The yoke on the rail, from the top](assembly/photos/10_0.jpg) | ![The nut bar under the rail](assembly/photos/10_1.jpg) |
| The yoke on the rail, from the top | The nut bar under the rail |

## 11. Cover

![Cover](assembly/renders/10_cover.png)

Slide the cover (03) on from the back; the wires from the mount come in
through the openings at its front edge. Fix it with 4 x M3x10. To open it
later: the four screws out, lift the cover about 2 mm, slide it back.

| | |
|---|---|
| ![The harness laid into the housing before closing](assembly/photos/11_0.jpg) | ![Cover on, yoke legs in their openings](assembly/photos/11_1.jpg) |
| The harness laid into the housing before closing | Cover on, yoke legs in their openings |

## 12. Lamp onto the housing

![Lamp onto the housing](assembly/renders/11_yoke_to_housing.png)

Set the yoke's legs through the openings in the cover onto the two tall posts
and fix them with 2 x M3x25 from the top.

![The finished lamp](assembly/photos/12_0.jpg)

## 13. On the monitor

![On the monitor](assembly/renders/12_on_monitor.png)
![Adjusting the depth](assembly/renders/12b_adjust.png)

Hang the lamp on the monitor's top edge. Optional: small 1 mm self-adhesive
silicone or felt pads on the front hook and on the two rear contact pads
protect the monitor's finish. Adjust for your monitor (8.2 to 34.2 mm at the
top edge): loosen the two M3x12 screws by one or two turns, slide the housing
until the rear pads touch the back of the monitor, tighten. All from the top.

| | |
|---|---|
| ![Hanging it on](assembly/photos/13_0.jpg) | ![On the monitor, from the side](assembly/photos/13_1.jpg) |
| Hanging it on | On the monitor, from the side |
| ![From behind](assembly/photos/13_2.jpg) | ![Lit](assembly/photos/13_3.jpg) |
| From behind | Lit |

![Lit, closer](assembly/photos/13_4.jpg)

The effect on the desk, bar on (left) and off (right):

![Desk with the bar on vs off](assembly/photos/led_on_vs_off.jpg)

Video of the finished bar in use: [YouTube](https://youtube.com/shorts/RAWoSDGTpJ0)

Then continue with [First setup](../README.md#first-setup) in the README.
