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
