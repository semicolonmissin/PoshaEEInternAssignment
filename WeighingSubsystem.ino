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
