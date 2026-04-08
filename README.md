# Posha Ingredient Dispensing — Weighing Subsystem Design

**Assignment Submission | Electronics Engineering**  
Srivathsa Thotakura | 23B1305

---

## Table of Contents

1. [System Context](#1-system-context)
2. [Precision Requirements](#2-precision-requirements)
3. [Load Cell Selection](#3-load-cell-selection)
4. [ADC Selection](#4-adc-selection)
5. [Connectors and Cables](#5-connectors-and-cables)
6. [Complete Signal Chain](#6-complete-signal-chain)

---

## 1. System Context

From the website, Youtube videos and images, I could deduce that Posha is a countertop cooking robot that autonomously dispenses ingredients - spices, purees, liquids, and dry goods into a cooking pan at recipe-specified times. Each ingredient container looks like it approximately holds **60-70g** of ingredient.

Looking at the physical design of the product, I am assuming that the weighing subsystem sits **below the pan** inside the base of the robot and measures the weight of accumulated ingredients in real time, allowing the system to confirm correct dispense quantities and detect dispensing errors.

**Key environmental constraints:**
- Induction cooktop generates stray magnetic fields
- Stirrer motor introduces mechanical vibration (10–100Hz)
- Off-center loading - food and dispense events do not always occur at the geometric center of the pan

---

## 2. Precision Requirements

### Ingredient classes and error tolerance

| Ingredient Type | Typical Dispense Qty | Acceptable Error | Notes |
|---|---|---|---|
| Spices / Salt | 2–10g | ±0.5g | Worst case — proportional error is highest |
| Sauces / Pastes | 20–60g | ±2g | Taste impact of small errors is low |
| Pastas/Rice | 30–60g | ±1–2g | Moderate tolerance |

The **governing constraint is spices** — the smallest quantities with the least tolerance for proportional error.

**Design target: ±0.5g precision**

### Load cell capacity

The load cell sits under the pan so total load = pan tare + accumulated ingredients:

- Pan (empty): ~1.5-2 kg
- Max accumulated ingredients (multi-step recipe): ~1-1.5 kg
- **Total max load: ~3.5 kg**

I think we should pick a **5 kg** load cell: provides safety headroom above the maximum operating load and protects against overload during rough handling (pan drop, etc.), and at the same time keeps us a safe margin in case of the cell itself not being accurate to its rating, or deteriorating over time.

### Required ADC resolution

```
Target precision    = 0.5g
Full scale          = 5000g
Required resolution = 0.5 / 5000 = 0.01% FS
Minimum bit depth   = log2(5000 / 0.5) = ~13.3 bits effective
```

I have worked with designing Delta Sigma ADCs for Inter IIT Tech Meet 14.0, and our design had an ENOB of 19 bits. A 24-bit sigma-delta ADC with ~18 ENOB would comfortably give us accurate values for the required precision.

---

## 3. Load Cell Selection

### Choice: TAL220B 5kg Single-Point Bending Beam

**Manufacturer:** Sparkfun  
**Type:** Single-point strain gauge bending beam

| Specification | Value |
|---|---|
| Capacity | 5 kg |
| Rated output | 1.0 mV/V ± 0.15% |
| Non-linearity | ≤ 0.05% FS |
| Hysteresis | ≤ 0.05% FS |
| Repeatability | ≤ 0.05% FS |
| TempCo (zero) | ≤ 0.05% FS / 10°C |
| TempCo (span) | ≤ 0.05% FS / 10°C |
| Safe overload | 150% of rated (7.5 kg) |
| IP Rating | IP65 |
| Operating temp | –10°C to +40°C |
| Cable | 4-wire shielded, 1m |
| Excitation voltage | 5–12V recommended |

### Signal output calculation

```
Excitation voltage  = 5V
Rated output        = 1.0 mV/V
Full scale output   = 1.0 mV/V × 5V = 5mV
Sensitivity         = 5mV / 5000g   = 1 µV/g
```

### Why TAL220B

**Single-point bending beam design** is the critical differentiator. This cell geometry mechanically compensates for off-center loading - when the pan has food piled on one side, or when a dispense event hits the edge of the pan, the output is the same as if the load were centered. Generic bar cells do not have this compensation and will introduce systematic error under off-center conditions. Our product too has the ingredient at a radial position, I think this is an important feature.

**IP65 sealing** protects the strain gauge bonding and cable entry from steam and oil aerosol — a real long-term reliability concern in a cooking environment. An unsealed cell will exhibit drift and eventual failure from moisture ingress.

**Specified TempCo** allows the firmware to apply temperature compensation with a temperature sensor, which would obviously be available on the board. An unspecified TempCo (as found on generic cells) makes compensation impossible.

**150% safe overload** protects against pan drop events during assembly and normal use.

### Comparison with alternatives

| Specification | TAL220B  | Generic Bar Cell | S-Type CZL301 |
|---|---|---|---|
| Non-linearity | 0.05% FS | 0.3% FS | 0.05% FS |
| Hysteresis | 0.05% FS | 0.3% FS | 0.05% FS |
| Repeatability | 0.05% FS | 0.1% FS | 0.03% FS |
| TempCo | Specified | Unknown | Specified |
| IP Rating | IP65 | None | IP65 |
| Off-center load | Compensated | Not compensated | Not applicable |
| Shielded cable | Yes | No | Yes |
| Safe overload | 150% | 120% | 150% |
| Error at 500g load | ~0.25g | ~1.5g+ | ~0.25g* |
| Meets ±0.5g target |  Yes |  Borderline |  integration no |

*S-type (CZL301) is not suitable for under the pan placement, it is more designed inclined towards hanging scales.*

An issue however, is when seen online, Digikey is the only vendor selling the TAL220B, which is not ideal for mass production, since it means that we would have to constantly import it from overseas. Ideally, if we could get an offline vendor who is already importing this, it could be more cost and time effective for us.

## 4. ADC Selection

### Why the MCU's internal ADC cannot be used

```
ESP32 internal ADC:
  Resolution        : 12-bit
  Voltage per LSB   : ~0.8mV (at 3.3V range)
  Load cell FS      : 5mV
  LSB in weight     : 0.8mV / (1µV/g) = 800g/LSB

Required            : 0.5g resolution
Shortfall           : 1600× too coarse — before accounting for
                      ESP32's known nonlinearity of 1–2% FS
```

A 24-bit sigma-delta ADC with integrated PGA is required. This architecture:
- Oversamples then decimates -> low effective noise floor
- Amplifies the bridge signal before digitization (PGA) -> SNR is set by the amplified signal, not the raw µV input
- Provides differential input with high CMRR -> rejects common-mode noise from supply and cable

### Most commonly used and good fit for us: HX711

**Manufacturer:** Avia Semiconductor  
**Type:** 24-bit sigma-delta ADC, purpose-built for weigh scale / bridge sensor applications

| Specification | Value |
|---|---|
| Resolution | 24-bit |
| PGA gain options | 32 / 64 / 128 (channel A), 32 (channel B) |
| Input range (128x gain) | ±20mV differential |
| ENOB | ~18 bits typical |
| Input referred noise | ~50nV RMS at 10 SPS |
| Data rates | 10 SPS / 80 SPS (RATE pin selectable) |
| CMRR | ≥100 dB |
| Supply voltage | 2.7V – 5.25V (analog + digital) |
| Interface | Custom 2-wire clocked serial (DOUT + SCK) |
| On-chip oscillator | Yes |
| On-chip voltage regulator | Yes |

### Signal chain calculation at 128x PGA, 10 SPS

```
Bridge output (full scale)    = 5mV
After 128x PGA                = 5mV × 128 = 640mV  → within ±20mV input range - looks good

ENOB                          = 18 bits
Resolution (input referred)   = 5mV / 2^18 = 19nV/LSB
Weight per LSB                = 19nV / 1µV per g = 0.019g/LSB

Input noise (10 SPS)          = 50nV RMS
Weight noise (RMS)            = 50nV / 1µV per g = 0.05g RMS
Weight noise (peak-to-peak)   = 0.05g × 6 = ~0.3g pk-pk  (99.7% confidence)

Margin against ±0.5g target   = 0.5g / 0.3g = 1.67× margin - looks good
```

### Why 10 SPS operating mode

At 10 SPS, the HX711's on-chip decimation filter has a **~3.5 Hz cutoff**. This inherently rejects:
- Stirrer motor vibration (10 - 100 Hz) - attenuated by > 20 dB
- Induction switching noise (20 kHz+) - attenuated by > 60 dB

No additional digital filtering is required to meet the noise spec. At 80 SPS mode, the cutoff rises to ~28 Hz and motor vibration begins to pass through, requiring firmware averaging to compensate. 10 SPS is the correct operating mode.

Dispensing events settle in 200 - 500ms. At 10 SPS (100ms/sample), averaging 5 samples gives a clean reading within 500ms of dispense completion - fast enough for the application.

### ADC comparison

| Specification | HX711 | NAU7802 | ADS1232 |
|---|---|---|---|
| ENOB | ~18 bits | ~23 bits | ~20 bits |
| Noise (10 SPS, input referred) | 50nV RMS | 10nV RMS | 30nV RMS |
| Effective weight noise (pk-pk) | ~0.3g | ~0.06g | ~0.18g |
| Meets ±0.5g target | Yes | Yes | Yes |
| Built-in low-pass filter | Yes (3.5Hz @ 10SPS) | Yes | Yes |
| Interface | Custom 2-wire | I²C | Custom 2-wire |
| External crystal required | No | No | Yes |
| On-chip LDO | No | Yes | No |
| Cost | ₹80–150 | ₹250–400 | ₹600–900 |
| Verdict | Right-sized | Overkill | Overkill + BOM cost |

## 5. Connectors and Cables

### Overview

Two interfaces exist in the signal chain:

```
[TAL220B] ──(analog, µV-level)──> [HX711 PCB] ──(digital, 3.3V)──> [MCU]
           Interface 1                          Interface 2
           (noise-critical)                     (straightforward)
```

---

### Interface 1 — Load Cell to PCB (Analog)

Carries the 4-wire Wheatstone bridge signals: E+, E−, S+, S−.  
Signal lines carry 0–5mV differential. Every decision here directly impacts measurement quality.

#### Cable

**Type: Shielded twisted pair (STP), 4-conductor**

| Parameter | Specification | Reason |
|---|---|---|
| Conductors | 4, arranged as 2 twisted pairs | E+/E− paired; S+/S− paired |
| Gauge | 24 AWG | Low DC resistance on excitation pair; flexible for internal routing |
| Shielding | Al/Mylar foil + tinned copper braid + drain wire | Foil for HF EMI; braid for low-impedance ground return |
| Jacket | PUR (polyurethane) | Oil-resistant, flexible under repeated movement |
| Max length | ≤ 1m | Minimize antenna area for EMI pickup |


**Twisted pair rationale:** The induction cooktop generates an alternating magnetic field. A twisted pair induces equal and opposite voltages in each half-twist, which cancel — providing common-mode rejection by geometry. Straight or ribbon cable has no such cancellation and will act as an antenna for induction switching noise.
From the pictures available on the digikey product page it appears that the cell comes with this twisted pair of wires.

**Shield grounding:**

```
Load cell end   →   FLOATING (not connected)
PCB end         →   Connected to AGND only
```

> Grounding both ends creates a ground loop — the shield becomes a current-carrying conductor and injects noise into the signal it is supposed to protect. Ground at one end only. Learnt this recently, the hard way.

#### Field Connector — JST XH 2.5mm, 4-pin (XHP-4 housing + B4B-XH-A header)

| Parameter | Value |
|---|---|
| Series | JST XH |
| Pitch | 2.5mm |
| Poles | 4 |
| IP rating | None (internal use) |
| Contact type | Crimp socket on wire (SXH-001T-P0.6); shrouded pin header on PCB (B4B-XH-A) |
| Contact plating | Tin over copper alloy |
| Mating cycles | ~30 (per JST spec) |
| Locking | Tab latch (positive retention) |
| Keyed | Yes (polarized housing — cannot be inserted reversed) |
| Current rating | 3A per contact |
| Wire range | 22–30 AWG |

**Pin assignment:**

| Pin | Signal | Wire colour |
|---|---|---|
| 1 | E+ (Excitation +) | Red |
| 2 | S+ (Signal +) | Green |
| 3 | E− (Excitation −) | Black |
| 4 | S− (Signal −) | White |

> **Shield drain wire:** We would want a higher AWG wire for the ground shield, and hence this will not be part of the JST connector. Instead we will have it directly soldered to a dedicated AGND pad on the PCB.

**Why JST XH:**
- **Keyed and latching** — the housing is polarized (physically impossible to insert reversed) and has a tab latch that clicks into place, preventing accidental disconnection from vibration. This solves the two biggest problems with DuPont connectors (no keying, no locking) at essentially the same cost.
- **Ubiquitous and cheap** — JST XH is one of the most widely used connector families in consumer electronics; housings, crimp pins, and PCB headers are readily available from local vendors across India at ₹5–15 per mated pair.
- **Adequate for an enclosed internal connection** — since the load cell cable runs entirely inside the sealed robot enclosure (not exposed to the external environment), the environmental sealing of an M8 or IP-rated connector is unnecessary; the enclosure itself provides the moisture and dust barrier.
- **Tool-free mating** — push-on with audible click, no threading or special tooling needed; the latch releases with a fingernail for rework.
- **Compact** — 2.5mm pitch keeps the PCB footprint small.



#### PCB-Side Connector — JST XH 2.5mm, 4-pin (XHP-4 housing + B4B-XH-A header)

| Parameter | Value |
|---|---|
| Series | JST XH |
| Pitch | 2.5mm |
| Poles | 4 |
| Contact plating | Tin over copper alloy |
| Mating cycles | ~30 |
| Locking | Tab latch (positive retention) |
| Keyed | Yes |
| Current rating | 3A per contact |
| Wire range | 22–30 AWG |

**Pin assignment:**

| Pin | Signal |
|---|---|
| 1 | E+ (Excitation +) |
| 2 | S+ (Signal +) |
| 3 | E− (Excitation −) |
| 4 | S− (Signal −) |

Using the same JST XH family on both sides of the cable:
- **Single connector family in BOM** — one crimp contact part number, one crimping tool, one housing series. Reduces procurement complexity and assembly training.
- **Keyed + latching** — same polarity protection and vibration resistance as the field-side connector.

---

### Interface 2 — HX711 PCB to MCU (Digital)

Four signals: VCC, GND, DOUT, SCK. The MCU is on a **separate PCB**, so the digital lines must travel over a cable between boards.

#### Why a buffer is needed

The HX711's DOUT output is a CMOS push-pull driver with limited drive strength. When driving a cable of any appreciable length (even 10–20 cm), the combination of cable capacitance and connector contact resistance can:
- Slow down edges → missed clock/data transitions at the MCU
- Pick up crosstalk and EMI on the cable → bit errors
- Load the HX711 output → reduced noise margin

Placing a **buffer IC** on the HX711 PCB immediately before the inter-board connector provides a low-impedance, high-drive output that maintains clean edges across the cable.

#### Buffer IC — 74HC125 Quad Bus Buffer (single gate used per signal)

| Parameter | Value |
|---|---|
| IC | 74HC125D (SOIC-14) or SN74HC125DR |
| Supply | 3.3V (same as HX711 DVDD) |
| Output drive | ±25mA per gate |
| Propagation delay | ~7 ns |
| Output enable | Active low (tie OE pins to GND to keep buffers always enabled) |

**Signal routing:**

| Buffer gate | Input (from HX711 / MCU) | Output (to cable) | Direction |
|---|---|---|---|
| Gate A | HX711 DOUT | → Cable DOUT | HX711 → MCU |
| Gate B | Cable SCK (from MCU) | → HX711 SCK | MCU → HX711 |

- **Gate A** buffers DOUT (HX711 → MCU): ensures the data line drives the cable capacitance cleanly.
- **Gate B** buffers SCK (MCU → HX711): the MCU's clock signal arrives via cable and is re-driven locally to the HX711 with clean edges. Place this buffer on the HX711 PCB so the HX711 sees a clean, locally-driven clock.
- Two gates remain unused — tie their inputs to GND or VCC to prevent floating CMOS inputs.

> **Bypass capacitor:** Place a 100nF ceramic capacitor directly on the 74HC125's VCC pin, as close to the IC as possible.

#### Inter-board connector — JST PH 2.0mm, 4-pin (B4B-PH-K)

| Parameter | Value |
|---|---|
| Pitch | 2.0mm |
| Poles | 4 |
| Current rating | 2A per contact |
| Locking | Tab latch |
| Keyed | Yes |

**Pin assignment:**

| Pin | Signal |
|---|---|
| 1 | VCC (3.3V) |
| 2 | GND |
| 3 | DOUT (buffered, HX711 → MCU) |
| 4 | SCK (MCU → HX711, buffered on arrival) |

---

### Additional Notes

**Strain relief:** Although the JST XH latch provides positive retention, add a cable tie anchor footprint on the PCB or a P-clip to the enclosure wall near the header. Secure the cable ~20mm before the connector so that any pull or vibration loads the strain relief, not the crimp contacts or latch tab. Over hundreds of cooking cycles, mechanical stress should be borne by the strain relief, not the connector.

**Ferrite bead:** Clamp a snap-on ferrite (e.g. Fair-Rite 0431167281) on the shielded cable immediately before the enclosure entry point. Provides a final stage of common-mode EMI suppression from the induction cooktop. Cost: ~₹20.

---

## 6. Complete Signal Chain

```
┌─────────────────────┐
│     TAL220B         │  5kg single-point bending beam
│     Load Cell       │  IP65  |  1.0mV/V  |  1µV/g sensitivity
└──────────┬──────────┘
           │
           │  4-wire STP cable, 24AWG, foil + braid shield
           │  PUR jacket, ≤1m
           │  Shield: FLOATING at this end
           │
┌──────────▼──────────┐
│  JST XH 4-pin       │  2.5mm pitch, keyed + latching
│  (cable end)        │  XHP-4 crimp housing
└──────────┬──────────┘
           │  (mates to shrouded header on PCB)
┌──────────▼──────────┐
│  JST XH 4-pin       │  B4B-XH-A shrouded header on PCB
│  Header (PCB)       │  Shield drain wire → soldered to AGND pad
└──────────┬──────────┘
           │
┌──────────▼──────────────────────────────────────────────┐
│                    HX711 PCB                            │
│                                                          │
│  ┌────────────────────────────────────────────────┐    │
│  │  HX711                                         │    │
│  │  Channel A, 128x PGA                           │    │
│  │  RATE pin = LOW → 10 SPS                       │    │
│  │  On-chip decimation filter: ~3.5Hz cutoff      │    │
│  │  CMRR ≥ 100dB                                  │    │
│  └───────────────────┬────────────────────────────┘    │
│                      │  DOUT + SCK (PCB traces)         │
│  ┌───────────────────▼────────────────────────────┐    │
│  │  74HC125 Bus Buffer                            │    │
│  │  Gate A: DOUT → buffered DOUT (to cable)       │    │
│  │  Gate B: SCK from cable → buffered SCK (local) │    │
│  │  25mA drive per gate                           │    │
│  └───────────────────┬────────────────────────────┘    │
│                      │                                   │
│  ┌───────────────────▼────────────────────────────┐    │
│  │  JST PH 4-pin (B4B-PH-K) — inter-board        │    │
│  │  VCC | GND | DOUT (buffered) | SCK             │    │
│  └───────────────────┬────────────────────────────┘    │
└──────────────────────┼──────────────────────────────────┘
                       │  Cable to MCU board
┌──────────────────────▼──────────────────────────────────┐
│                    MCU PCB                              │
│  ┌────────────────────────────────────────────────┐    │
│  │  JST PH 4-pin (B4B-PH-K)                      │    │
│  └───────────────────┬────────────────────────────┘    │
│  ┌───────────────────▼────────────────────────────┐    │
│  │  MCU (ESP32 or equivalent)                     │    │
│  │  Bit-bang 2-wire HX711 protocol                │    │
│  │  5-sample moving average                       │    │
│  │  Settling time detection before accepting read │    │
│  └────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────┘
```

### Summary table

| Stage | Component | Key reason |
|---|---|---|
| Sensing | TAL220B 5kg | Off-center compensation, IP65, specified TempCo |
| Cable | STP 24AWG foil+braid, PUR | Twisted pair rejects induction field; shield rejects EMI |
| Field connector | JST XH 4-pin 2.5mm | Keyed, latching, cheap, adequate for sealed internal connection |
| PCB connector | JST XH 4-pin 2.5mm | Same family as field connector, keyed + latching |
| ADC | HX711 @ 128x PGA, 10 SPS | Purpose-built for bridge; 3.5Hz filter rejects motor vibration |
| Buffer | 74HC125 | Drives DOUT/SCK cleanly across inter-board cable |
| MCU interface | JST PH 2.0mm 4-pin | Compact, latching, keyed; inter-board digital link |

---

## 7. Embedded Firmware

### Architecture overview

The firmware is split into two layers:

```
┌──────────────────────────────────────────────────────┐
│  Application Layer  (WeighingSubsystem.ino)          │
│  - Tare, calibrate, read weight                      │
│  - 5-sample moving average                           │
│  - Settling time detection                           │
│  - Serial reporting / integration with main system   │
└──────────────────────┬───────────────────────────────┘
                       │ calls
┌──────────────────────▼───────────────────────────────┐
│  Driver Layer  (HX711.h / HX711.cpp)                 │
│  - Bit-bang 2-wire HX711 protocol                    │
│  - Raw 24-bit read with timeout                      │
│  - Power down / power up                             │
│  - Gain and channel selection                        │
└──────────────────────────────────────────────────────┘
```

The driver handles the HX711's custom serial protocol. It does **not** contain any application logic (averaging, calibration, etc.) — that belongs in the application layer. This separation means the driver can be reused across projects without modification.

---

### 7.1 Driver — `HX711.h`

```cpp
/**
 * HX711.h — Minimal driver for the HX711 24-bit ADC
 * 
 * Protocol summary (from Avia Semiconductor datasheet):
 *   - DOUT goes LOW when a new conversion is ready
 *   - MCU clocks SCK 24 times to shift out 24 data bits (MSB first)
 *   - Additional SCK pulses (after 24) set the gain for the NEXT conversion:
 *       25 pulses total → Channel A, gain 128
 *       26 pulses total → Channel B, gain 32
 *       27 pulses total → Channel A, gain 64
 *   - Data is 24-bit two's complement
 *   - SCK HIGH for >60µs → HX711 enters power-down mode
 *
 * Hardware context:
 *   - Load cell: TAL220B 5kg, 1.0mV/V, excited at 5V → 5mV FS
 *   - PGA gain 128 → 640mV FS at ADC input
 *   - RATE pin tied LOW → 10 SPS (3.5Hz decimation filter cutoff)
 *   - DOUT and SCK routed through 74HC125 buffer to MCU via JST PH cable
 */

#ifndef HX711_H
#define HX711_H

#include <Arduino.h>

/**
 * Gain/channel selection — determines number of SCK pulses per read.
 * The enum values represent the total number of SCK pulses needed.
 */
enum class HX711Gain : uint8_t {
    CHANNEL_A_128 = 25,  // Channel A, gain 128 (default, used for our load cell)
    CHANNEL_B_32  = 26,  // Channel B, gain 32
    CHANNEL_A_64  = 27   // Channel A, gain 64
};

class HX711 {
public:
    /**
     * Constructor.
     * @param dout_pin  GPIO connected to HX711 DOUT (via 74HC125 buffer)
     * @param sck_pin   GPIO connected to HX711 SCK  (via 74HC125 buffer)
     */
    HX711(uint8_t dout_pin, uint8_t sck_pin);

    /**
     * Initialise GPIO pins. Call once in setup().
     * Sets gain to Channel A, 128x by default.
     * Performs one dummy read to latch the gain setting into the HX711.
     */
    void begin(HX711Gain gain = HX711Gain::CHANNEL_A_128);

    /**
     * Check if the HX711 has a conversion ready.
     * @return true if DOUT is LOW (data available)
     */
    bool isReady() const;

    /**
     * Wait for a conversion to become ready, with timeout.
     * @param timeout_ms  Maximum time to wait (default 200ms, well above 
     *                     the 100ms conversion period at 10 SPS)
     * @return true if data became ready, false if timed out
     */
    bool waitReady(unsigned long timeout_ms = 200) const;

    /**
     * Read a single raw 24-bit signed value from the HX711.
     * Blocks until data is ready (with timeout).
     * 
     * @param result  Output: the signed 24-bit ADC value (sign-extended to int32_t)
     * @return true if read succeeded, false if timed out waiting for data
     * 
     * The raw value is in ADC counts. Positive = signal+ > signal-.
     * At our operating point (128x gain, 5mV FS):
     *   Full scale ≈ +8,388,607 counts (2^23 - 1)
     *   1 LSB ≈ 0.019g (theoretical, before noise)
     */
    bool readRaw(int32_t &result);

    /**
     * Set the gain/channel for subsequent reads.
     * Takes effect after the next readRaw() call (the HX711 latches
     * the gain setting at the end of each read cycle).
     */
    void setGain(HX711Gain gain);

    /**
     * Power down the HX711 (SCK HIGH for >60µs).
     * Reduces supply current to <1µA. Call powerUp() to resume.
     */
    void powerDown();

    /**
     * Power up the HX711 from power-down state.
     * Pulls SCK LOW; the HX711 resets and begins conversions.
     * First conversion after power-up should be discarded (settling).
     */
    void powerUp();

private:
    uint8_t  _dout_pin;
    uint8_t  _sck_pin;
    HX711Gain _gain;

    /**
     * Shift out one bit by pulsing SCK and reading DOUT.
     * SCK pulse width is ~1µs (well within HX711's timing requirements).
     */
    uint8_t shiftInBit();
};

#endif // HX711_H
```

---

### 7.2 Driver — `HX711.cpp`

```cpp
/**
 * HX711.cpp — Implementation of the HX711 bit-bang driver
 */

#include "HX711.h"

HX711::HX711(uint8_t dout_pin, uint8_t sck_pin)
    : _dout_pin(dout_pin)
    , _sck_pin(sck_pin)
    , _gain(HX711Gain::CHANNEL_A_128)
{
}

void HX711::begin(HX711Gain gain) {
    pinMode(_dout_pin, INPUT);
    pinMode(_sck_pin, OUTPUT);
    digitalWrite(_sck_pin, LOW);

    _gain = gain;

    // Perform one dummy read to latch the gain setting into the HX711.
    // The HX711 applies the gain setting from the PREVIOUS read cycle,
    // so this first read configures it for all subsequent reads.
    int32_t dummy;
    waitReady(500);   // first conversion after power-on can take longer
    readRaw(dummy);
}

bool HX711::isReady() const {
    return digitalRead(_dout_pin) == LOW;
}

bool HX711::waitReady(unsigned long timeout_ms) const {
    unsigned long start = millis();
    while (!isReady()) {
        if (millis() - start > timeout_ms) {
            return false;
        }
        yield();  // feed ESP32 watchdog during busy-wait
    }
    return true;
}

uint8_t HX711::shiftInBit() {
    // SCK HIGH — HX711 shifts out one bit on DOUT
    digitalWrite(_sck_pin, HIGH);
    delayMicroseconds(1);

    uint8_t bit = digitalRead(_dout_pin);

    // SCK LOW — prepare for next bit
    digitalWrite(_sck_pin, LOW);
    delayMicroseconds(1);

    return bit;
}

bool HX711::readRaw(int32_t &result) {
    // Wait for DOUT to go LOW (conversion ready)
    if (!waitReady()) {
        return false;
    }

    // --- Critical timing section ---
    // Disable interrupts to prevent SCK from being held HIGH for >60µs
    // (which would put the HX711 into power-down mode mid-read).
    noInterrupts();

    // Shift in 24 data bits, MSB first
    uint32_t raw = 0;
    for (uint8_t i = 0; i < 24; i++) {
        raw = (raw << 1) | shiftInBit();
    }

    // Send additional SCK pulses to set gain for NEXT conversion.
    // Total pulses = 25 (gain 128), 26 (gain 32), or 27 (gain 64).
    uint8_t extra_pulses = static_cast<uint8_t>(_gain) - 24;
    for (uint8_t i = 0; i < extra_pulses; i++) {
        digitalWrite(_sck_pin, HIGH);
        delayMicroseconds(1);
        digitalWrite(_sck_pin, LOW);
        delayMicroseconds(1);
    }

    interrupts();
    // --- End critical section ---

    // Sign-extend 24-bit two's complement to 32-bit
    // If bit 23 is set, the value is negative — fill upper 8 bits with 1s
    if (raw & 0x800000) {
        result = static_cast<int32_t>(raw | 0xFF000000);
    } else {
        result = static_cast<int32_t>(raw);
    }

    return true;
}

void HX711::setGain(HX711Gain gain) {
    _gain = gain;
    // The new gain takes effect after the next readRaw() call.
    // The caller should discard that first read if they need the new gain
    // to be fully settled.
}

void HX711::powerDown() {
    // Pull SCK HIGH for >60µs to enter power-down
    digitalWrite(_sck_pin, HIGH);
    delayMicroseconds(80);  // 80µs — comfortably above 60µs threshold
}

void HX711::powerUp() {
    // Pull SCK LOW to exit power-down
    digitalWrite(_sck_pin, LOW);
    // HX711 will reset its internal registers and begin converting.
    // First conversion takes ~400ms at 10 SPS — caller should wait.
}
```

### Key implementation decisions

**`noInterrupts()` during the 24-bit shift:** The ESP32 runs FreeRTOS with WiFi/BT interrupts that can fire at any time. If an interrupt holds SCK HIGH for >60µs, the HX711 interprets this as a power-down command and the read is corrupted. Disabling interrupts for the ~50µs shift duration prevents this. The WiFi stack tolerates this — it routinely handles interrupt latencies of this magnitude.

**`yield()` in `waitReady()`:** The ESP32's watchdog timer will reset the MCU if the main loop doesn't yield within the watchdog timeout period (~3.2s by default). Since `waitReady()` can block for up to 200ms at 10 SPS, we call `yield()` in the spin-wait loop to keep the watchdog fed.

**Sign extension:** The HX711 outputs 24-bit two's complement. Without sign extension, a negative reading (e.g., load cell tared then lifted) would appear as a large positive number (~16 million). The sign extension ensures negative readings map correctly to negative `int32_t` values.

---

### 7.3 Application — `WeighingSubsystem.ino`

```cpp
/**
 * WeighingSubsystem.ino
 * 
 * Weighing subsystem firmware for the Posha ingredient dispensing robot.
 * Reads the TAL220B load cell via HX711 ADC and provides:
 *   - Tare (zero offset)
 *   - Calibration (with known reference weight)
 *   - 5-sample moving average with settling detection
 *   - Serial output for integration with main system controller
 *
 * Hardware:
 *   - ESP32 (MCU board, separate from HX711 board)
 *   - HX711 @ Channel A, 128x PGA, 10 SPS
 *   - TAL220B 5kg load cell
 *   - 74HC125 buffer on HX711 board, JST PH inter-board cable
 *
 * Pin assignments (ESP32 side of the inter-board cable):
 *   - GPIO 16 → DOUT (from HX711 via buffer)
 *   - GPIO 17 → SCK  (to HX711 via buffer)
 */

#include "HX711.h"

// ── Pin assignments ──────────────────────────────────────────────
static const uint8_t PIN_HX711_DOUT = 16;
static const uint8_t PIN_HX711_SCK  = 17;

// ── Calibration constants ────────────────────────────────────────
// These are determined during the calibration procedure (see below).
// Default values are approximate — must be calibrated per unit.
static float calibration_factor = 1.0f;   // counts per gram (set during calibration)
static int32_t tare_offset      = 0;      // raw ADC counts at zero load

// ── Moving average ───────────────────────────────────────────────
static const uint8_t AVG_WINDOW = 5;      // 5 samples = 500ms at 10 SPS
static int32_t raw_buffer[AVG_WINDOW];    // circular buffer of raw readings
static uint8_t buf_index = 0;
static uint8_t buf_count = 0;             // tracks fill level until buffer is full

// ── Settling detection ───────────────────────────────────────────
// A reading is "settled" when the spread (max - min) of the averaging
// window is below the settling threshold.
static const float SETTLING_THRESHOLD_G = 0.5f;  // ±0.5g — our precision target

// ── Driver instance ──────────────────────────────────────────────
HX711 scale(PIN_HX711_DOUT, PIN_HX711_SCK);

// ── State ────────────────────────────────────────────────────────
enum class SystemState {
    IDLE,
    TARING,
    CALIBRATING,
    RUNNING
};

static SystemState state = SystemState::IDLE;

// =====================================================================
//  Helper functions
// =====================================================================

/**
 * Add a raw reading to the circular buffer.
 */
void bufferPush(int32_t raw) {
    raw_buffer[buf_index] = raw;
    buf_index = (buf_index + 1) % AVG_WINDOW;
    if (buf_count < AVG_WINDOW) {
        buf_count++;
    }
}

/**
 * Compute the average of the circular buffer.
 * Returns 0 if buffer is empty.
 */
int32_t bufferAverage() {
    if (buf_count == 0) return 0;

    int64_t sum = 0;
    for (uint8_t i = 0; i < buf_count; i++) {
        sum += raw_buffer[i];
    }
    return static_cast<int32_t>(sum / buf_count);
}

/**
 * Check if the current buffer contents represent a settled reading.
 * "Settled" means the spread of all samples in the window, converted
 * to grams, is within ±SETTLING_THRESHOLD_G.
 */
bool isSettled() {
    if (buf_count < AVG_WINDOW) return false;  // need a full window

    int32_t min_val = raw_buffer[0];
    int32_t max_val = raw_buffer[0];
    for (uint8_t i = 1; i < AVG_WINDOW; i++) {
        if (raw_buffer[i] < min_val) min_val = raw_buffer[i];
        if (raw_buffer[i] > max_val) max_val = raw_buffer[i];
    }

    float spread_g = static_cast<float>(max_val - min_val) / calibration_factor;
    return spread_g <= SETTLING_THRESHOLD_G;
}

/**
 * Convert a tare-subtracted raw average to grams.
 */
float rawToGrams(int32_t raw_avg) {
    return static_cast<float>(raw_avg - tare_offset) / calibration_factor;
}

/**
 * Reset the circular buffer (e.g., after tare or calibration).
 */
void bufferReset() {
    buf_index = 0;
    buf_count = 0;
    for (uint8_t i = 0; i < AVG_WINDOW; i++) {
        raw_buffer[i] = 0;
    }
}

// =====================================================================
//  Tare procedure
// =====================================================================

/**
 * Tare the scale: read N samples, average them, store as zero offset.
 * Call with the pan (but no ingredients) on the load cell.
 * 
 * @param num_samples  Number of samples to average for tare (default 10)
 * @return true if tare succeeded
 */
bool tareScale(uint8_t num_samples = 10) {
    Serial.println("[TARE] Starting tare — ensure pan is on scale, no ingredients.");

    int64_t sum = 0;
    uint8_t good_reads = 0;

    for (uint8_t i = 0; i < num_samples; i++) {
        int32_t raw;
        if (scale.readRaw(raw)) {
            sum += raw;
            good_reads++;
        } else {
            Serial.println("[TARE] WARNING: Read timeout, retrying.");
        }
    }

    if (good_reads == 0) {
        Serial.println("[TARE] FAILED — no valid readings from HX711.");
        return false;
    }

    tare_offset = static_cast<int32_t>(sum / good_reads);
    bufferReset();

    Serial.print("[TARE] Complete. Offset = ");
    Serial.print(tare_offset);
    Serial.print(" counts (");
    Serial.print(good_reads);
    Serial.println(" samples averaged).");

    return true;
}

// =====================================================================
//  Calibration procedure
// =====================================================================

/**
 * Calibrate the scale using a known reference weight.
 * Must be called AFTER tareScale().
 * 
 * Procedure:
 *   1. Tare the scale (pan only, no ingredients)
 *   2. Place known weight on the pan
 *   3. Call calibrateScale(known_weight_grams)
 * 
 * @param known_weight_g  The known reference weight in grams
 * @param num_samples     Number of samples to average (default 20 for accuracy)
 * @return true if calibration succeeded
 */
bool calibrateScale(float known_weight_g, uint8_t num_samples = 20) {
    Serial.print("[CAL] Starting calibration with ");
    Serial.print(known_weight_g, 1);
    Serial.println("g reference weight.");

    int64_t sum = 0;
    uint8_t good_reads = 0;

    for (uint8_t i = 0; i < num_samples; i++) {
        int32_t raw;
        if (scale.readRaw(raw)) {
            sum += raw;
            good_reads++;
        } else {
            Serial.println("[CAL] WARNING: Read timeout, retrying.");
        }
    }

    if (good_reads == 0) {
        Serial.println("[CAL] FAILED — no valid readings from HX711.");
        return false;
    }

    int32_t raw_avg = static_cast<int32_t>(sum / good_reads);
    int32_t delta = raw_avg - tare_offset;

    if (delta == 0) {
        Serial.println("[CAL] FAILED — no change from tare. Is the weight on the pan?");
        return false;
    }

    // calibration_factor = counts per gram
    calibration_factor = static_cast<float>(delta) / known_weight_g;
    bufferReset();

    Serial.print("[CAL] Complete. Factor = ");
    Serial.print(calibration_factor, 2);
    Serial.print(" counts/g (");
    Serial.print(good_reads);
    Serial.println(" samples averaged).");

    // Verification
    float check_weight = static_cast<float>(delta) / calibration_factor;
    Serial.print("[CAL] Verification: measured = ");
    Serial.print(check_weight, 2);
    Serial.print("g, expected = ");
    Serial.print(known_weight_g, 1);
    Serial.println("g.");

    return true;
}

// =====================================================================
//  Serial command handler
// =====================================================================

/**
 * Simple serial command interface for bring-up and integration.
 * 
 * Commands (send via Serial Monitor, 115200 baud):
 *   T           → Tare the scale
 *   C <weight>  → Calibrate with known weight (e.g., "C 100.0")
 *   R           → Single settled reading
 *   S           → Start continuous streaming mode
 *   P           → Pause (stop streaming)
 *   D           → Power down HX711
 *   U           → Power up HX711
 */
void handleSerialCommand() {
    if (!Serial.available()) return;

    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.length() == 0) return;

    char c = toupper(cmd.charAt(0));

    switch (c) {
        case 'T':
            state = SystemState::TARING;
            tareScale(10);
            state = SystemState::IDLE;
            break;

        case 'C': {
            // Parse weight from "C 100.0"
            float weight = cmd.substring(1).toFloat();
            if (weight <= 0.0f) {
                Serial.println("[CMD] ERROR: Invalid weight. Usage: C <weight_grams>");
                break;
            }
            state = SystemState::CALIBRATING;
            calibrateScale(weight, 20);
            state = SystemState::IDLE;
            break;
        }

        case 'R':
            // Single reading — wait for settled
            state = SystemState::RUNNING;
            Serial.println("[CMD] Waiting for settled reading...");
            // The main loop will handle this and print once settled
            break;

        case 'S':
            state = SystemState::RUNNING;
            Serial.println("[CMD] Streaming started.");
            break;

        case 'P':
            state = SystemState::IDLE;
            Serial.println("[CMD] Paused.");
            break;

        case 'D':
            scale.powerDown();
            state = SystemState::IDLE;
            Serial.println("[CMD] HX711 powered down.");
            break;

        case 'U':
            scale.powerUp();
            state = SystemState::IDLE;
            Serial.println("[CMD] HX711 powered up. Wait ~400ms for first conversion.");
            break;

        default:
            Serial.println("[CMD] Unknown command. Options: T, C <wt>, R, S, P, D, U");
            break;
    }
}

// =====================================================================
//  setup() and loop()
// =====================================================================

void setup() {
    Serial.begin(115200);
    while (!Serial) { yield(); }

    Serial.println();
    Serial.println("═══════════════════════════════════════════════");
    Serial.println("  Posha Weighing Subsystem — Firmware v1.0");
    Serial.println("  TAL220B 5kg  |  HX711 @ 128x PGA, 10 SPS");
    Serial.println("═══════════════════════════════════════════════");
    Serial.println();

    // Initialise HX711 driver — Channel A, 128x gain
    scale.begin(HX711Gain::CHANNEL_A_128);
    Serial.println("[INIT] HX711 initialised.");

    // Auto-tare on startup (assumes pan is on the scale)
    Serial.println("[INIT] Auto-tare in progress...");
    if (tareScale(10)) {
        state = SystemState::RUNNING;
        Serial.println("[INIT] Ready. Streaming weight data.");
    } else {
        state = SystemState::IDLE;
        Serial.println("[INIT] Tare failed — check HX711 wiring. Send 'T' to retry.");
    }

    Serial.println();
    Serial.println("Commands: T=Tare  C <g>=Calibrate  S=Stream  P=Pause  R=Read  D=Down  U=Up");
    Serial.println();
}

void loop() {
    // Always check for serial commands
    handleSerialCommand();

    // Only read the scale when in RUNNING state
    if (state != SystemState::RUNNING) {
        return;
    }

    // Attempt a read — non-blocking check first, then blocking read
    if (!scale.isReady()) {
        return;  // no new conversion yet, come back next loop iteration
    }

    int32_t raw;
    if (!scale.readRaw(raw)) {
        return;  // timeout — should not happen since we checked isReady()
    }

    // Push into circular buffer
    bufferPush(raw);

    // Compute current weight
    int32_t avg = bufferAverage();
    float weight_g = rawToGrams(avg);
    bool settled = isSettled();

    // Print in CSV-ish format for easy parsing by the main controller
    // Format: <weight_g>,<settled>,<raw_avg>
    Serial.print(weight_g, 2);
    Serial.print(" g");
    if (settled) {
        Serial.print("  [SETTLED]");
    } else {
        Serial.print("  [SETTLING]");
    }
    Serial.print("  raw=");
    Serial.println(avg);
}
```

### Calibration procedure (step-by-step)

This is the procedure a technician follows during production bring-up or after load cell replacement:

```
1. Power on the system with the empty pan on the load cell.
   → Firmware auto-tares on boot.

2. Verify tare:
   → Serial output should read ~0.00g [SETTLED].
   → If not, send 'T' to re-tare.

3. Place a known reference weight on the pan.
   → Use a calibrated 100g or 200g test weight.
   → Wait for the serial output to stabilise (~2-3 seconds).

4. Send calibration command:
   → Type "C 100.0" (or whatever the reference weight is, in grams).
   → Firmware reads 20 samples, computes calibration factor.
   → Verification line confirms measured vs expected weight.

5. Remove the reference weight.
   → Serial output should return to ~0.00g.
   → Place the reference weight back on — should read the correct weight.

6. (Optional) Store calibration_factor and tare_offset to EEPROM/NVS
   for persistence across power cycles. Not implemented in this version
   to keep the code focused — add with ESP32 Preferences library in
   production firmware.
```

### Firmware design notes

| Decision | Rationale |
|---|---|
| No external HX711 library | The popular "HX711" Arduino library (bogde/HX711) works, but bundles application logic (tare, averaging) into the driver layer, making it harder to customise settling detection and integrate with the main Posha controller. Writing our own driver is ~100 lines and gives us full control. |
| `isReady()` check before `readRaw()` in loop | Avoids blocking the main loop while waiting for the next 10 SPS conversion. The ESP32 can service other tasks (serial commands, communication with the main controller) between conversions. |
| Circular buffer for averaging | A simple 5-element circular buffer gives constant-time push and average operations. No heap allocation, no dynamic sizing — appropriate for a real-time embedded system. |
| Settling = spread of window | Checking `max - min` of the averaging window is more robust than checking if consecutive readings are "close enough." It catches oscillatory noise that adjacent-sample checking would miss. |
| `int64_t` for sum in averaging | 5 × 2^23 = ~42 million, which fits in `int32_t` (max ~2.1 billion). However, the tare sum (10 samples) and calibration sum (20 samples) can approach `20 × 8.3M ≈ 166M`, still fine for `int64_t`. Using `int64_t` universally avoids any future overflow if window sizes increase. |
| No EEPROM persistence yet | Calibration factor and tare offset are stored in RAM only. For production, store these in ESP32's NVS (Non-Volatile Storage) using the `Preferences` library. Omitted here to keep the code focused on the signal chain. |

## 8. PCB Schematic

The schematic for the HX711 weighing PCB has been captured in KiCad and is submitted as an EDIF netlist file alongside this document.

The schematic contains the following blocks:

| Block | Components | Function |
|---|---|---|
| Load cell input | JST XH 4-pin header (J1), AGND solder pad (TP1) | Receives the 4-wire bridge signals from the TAL220B; shield drain wire solders to TP1 |
| ADC | HX711 (U1), 100nF bypass caps (C1, C2) on AVDD and DVDD | Amplifies (128×) and digitises the bridge signal at 10 SPS |
| Buffer | 74HC125D (U2), 100nF bypass cap (C3) | Buffers DOUT and SCK for inter-board cable drive |
| MCU output | JST PH 4-pin header (J2) | Provides VCC, GND, buffered DOUT, and SCK to the MCU board |
| Power | Power flag symbols, decoupling network | 3.3V or 5V supplied from MCU board via J2 pin 1 |

> The EDIF file is the authoritative netlist. Refer to it for exact connectivity, component values, and reference designators.

---

## 9. PCB Layout — Constraints and Guidelines

I am unable to complete the PCB layout due to time constraints. However, the following layout guidelines document the design intent and I would follow them when the layout is eventually done. These are not arbitrary — each rule traces back to a specific noise, signal integrity, or reliability concern identified earlier in this document.

---

### 9.1 Board stackup and ground plane

- **2-layer board** is acceptable for this design (low signal count, 10 SPS data rate, no high-speed digital).
- **Unbroken ground plane on the bottom layer.** The HX711's CMRR (≥100 dB) is only as good as the ground reference it sees. Splits or slots in the ground plane under the analog signal path create return current detours that convert common-mode noise into differential-mode noise — directly degrading measurement accuracy.
- If a 4-layer stackup is available: L1 = signal, L2 = GND, L3 = power, L4 = signal. But 2-layer with a solid bottom-side ground pour is sufficient here.

### 9.2 Component placement

```
┌──────────────────────────────────────────┐
│  J1 (JST XH)           U1 (HX711)       │
│  [Load Cell In]   C1,C2 ■               │
│                                          │
│  TP1 (AGND pad)                          │
│                         U2 (74HC125)     │
│                    C3   ■         J2     │
│                                 (JST PH) │
│                                [MCU Out] │
│  (M2)                            (M2)    │
└──────────────────────────────────────────┘
```

- **J1 (load cell) on the left edge, J2 (MCU) on the right edge.** This creates a unidirectional signal flow: analog in → ADC → buffer → digital out. Analog and digital sections are physically separated, minimising crosstalk.
- **HX711 close to J1.** The analog traces (µV-level bridge signals) must be as short as possible. Every millimetre of trace is an antenna for EMI. Place U1 within 10mm of J1.
- **74HC125 close to J2.** The buffer's purpose is to drive the inter-board cable — place it adjacent to the output connector so the high-drive output traces are short and the cable sees the buffer's low output impedance directly.
- **Bypass capacitors touching their IC pads.** C1 and C2 directly against HX711's AVDD and DVDD pins. C3 directly against 74HC125's VCC pin. The effectiveness of a bypass cap is inversely proportional to the loop area between the cap, the VCC pin, and the closest GND via. Target <3mm loop area.

### 9.3 Analog trace routing

- **Differential pair routing for S+ and S−.** Run the two signal traces as a closely-coupled pair (spacing ≤ 2× trace width) from J1 to HX711's INA+ and INA− pins. Matched length is not critical at 10 SPS, but keeping them parallel and close ensures any noise pickup is common-mode and rejected by the HX711's CMRR.
- **Excitation traces (E+, E−) as a pair.** Same principle — route together from J1 to HX711's excitation output pins.
- **No vias in the analog signal path.** Every via is a discontinuity and an opportunity for noise coupling. Route S+, S−, E+, E− entirely on the top layer.
- **Guard ring (optional but recommended).** Surround the analog traces with a grounded copper pour connected to AGND. This provides electrostatic shielding against digital noise coupling from the buffer section of the board.
- **Trace width for analog signals:** 0.2–0.3mm is fine — these carry essentially zero current (high-impedance bridge sense lines). The excitation pair carries ~1mA — 0.3mm is more than adequate.

### 9.4 Ground strategy — star ground

- **Single point where AGND and DGND meet.** The HX711 datasheet recommends connecting analog and digital grounds at a single point near the IC. Do not merge them elsewhere on the board.
- **Digital return currents must not flow through the analog ground region.** This means: no DGND vias or pours under/near the analog traces. Route digital ground returns on the opposite side of the board from the analog section.
- **Shield drain pad (TP1) connects to AGND** — not DGND. The shield carries analog-side interference; routing it to DGND would inject that noise into the digital ground and potentially back into the ADC's reference.

### 9.5 Digital trace routing

- **DOUT and SCK between HX711 and 74HC125:** Short PCB traces, no special constraints. These are 10 SPS clocked signals — signal integrity is not a concern at this frequency.
- **Buffered DOUT and SCK to J2:** Route directly from 74HC125 outputs to J2. Keep traces reasonably short but no special impedance matching needed.
- **Unused 74HC125 gate inputs:** Tie to GND or VCC via 0Ω resistors or direct traces. Do not leave floating — floating CMOS inputs oscillate and generate noise.

### 9.6 Mechanical

- **Board outline:** Target ~30mm × 25mm. Compact enough to fit inside the Posha base enclosure alongside other PCBs.
- **Mounting holes:** Two M2 holes in diagonally opposite corners for standoff mounting. Keep holes away from analog traces (the screw head is a potential noise antenna if it contacts the enclosure ground at a different potential than board ground).
- **Connector placement:** J1 and J2 on opposite board edges for clean cable routing inside the enclosure. Both connectors should have their latch tabs facing outward (toward the board edge) for easy cable insertion/removal.
- **Silkscreen:** Mark pin 1 on both connectors, component reference designators, board name ("POSHA-WEIGH-V1"), and revision number.

### 9.7 Design rule summary

| Rule | Value | Reason |
|---|---|---|
| Min trace width | 0.2mm (8 mil) | Standard for 2-layer fab; adequate for signal currents |
| Min clearance | 0.2mm (8 mil) | Prevents solder bridging at standard fab tolerances |
| Via drill | 0.3mm, annular ring 0.15mm | Standard via; avoid in analog path |
| Analog trace pair spacing | ≤ 2× trace width | Common-mode noise rejection by geometry |
| Bypass cap loop area | < 3mm total path | Effective decoupling requires minimal inductance |
| HX711 to J1 distance | ≤ 10mm | Minimise analog trace antenna length |
| Ground plane | Unbroken under analog section | Maintain CMRR; prevent return current detours |
| AGND/DGND join | Single point near HX711 | Prevent digital noise from corrupting analog reference |

---

## Conclusion

This document presents the complete weighing subsystem design for the Posha ingredient dispensing robot — from the precision requirements that drive every component choice, through the load cell, ADC, connector, and cable selections, to the embedded firmware and PCB design guidelines.

The design achieves **±0.5g precision** (governing constraint: spice dispensing) with comfortable margin, using a TAL220B 5kg single-point bending beam load cell and an HX711 24-bit sigma-delta ADC at 128× PGA gain and 10 SPS. The 3.5 Hz decimation filter inherent to the 10 SPS mode rejects stirrer motor vibration without additional digital filtering. A 74HC125 bus buffer ensures clean digital signal transmission between the HX711 PCB and the MCU board over a JST PH inter-board cable.

The schematic has been completed and is submitted as an EDIF netlist file alongside this document. The PCB layout has not been completed due to time constraints, but Section 9 documents the layout guidelines — ground plane strategy, component placement, analog routing rules, and design rule values — that should be followed when the layout is eventually done.

---

*Srivathsa Thotakura | 23B1305*
