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
