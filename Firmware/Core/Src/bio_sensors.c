#include "bio_sensors.h"

extern I2C_HandleTypeDef hi2c3;

#define BIOSENSORS_MAX_HEART_RATE_BPM            220U
#define BIOSENSORS_MIN_HEART_RATE_BPM            35U
#define BIOSENSORS_SPO2_MIN_PERCENT              70.0f
#define BIOSENSORS_SPO2_MAX_PERCENT              100.0f

typedef struct {
    float temperature_c;
    float heart_rate_bpm;
    float spo2_percent;
    uint32_t red;
    uint32_t ir;
    uint8_t max30101_available;
    uint8_t max30205_available;
    uint8_t temperature_valid;
    uint8_t heart_rate_valid;
    uint8_t spo2_valid;
} BioSensorsData;

typedef struct {
    BioSensorsData latest;
    uint32_t red_buffer[BIOSENSORS_PPG_BUFFER_SIZE];
    uint32_t ir_buffer[BIOSENSORS_PPG_BUFFER_SIZE];
    uint16_t write_index;
    uint16_t sample_count;
    volatile uint8_t ppg_ready_flag;
} BioSensorsContext;

static BioSensorsContext g_bio_sensors = {0};

static float BioSensors_AbsFloat(float value);
static float BioSensors_ClampFloat(float value, float min_value, float max_value);
static int16_t BioSensors_FloatToScaledInt16(float value);
static uint8_t BioSensors_ReadRegister8(uint8_t device_address, uint8_t register_address, uint8_t *value);
static uint8_t BioSensors_WriteRegister8(uint8_t device_address, uint8_t register_address, uint8_t value);
static uint8_t BioSensors_ReadBytes(uint8_t device_address, uint8_t register_address, uint8_t *buffer, uint16_t length);
static uint8_t BioSensors_ReadTemperature(float *temperature_c);
static uint8_t BioSensors_ResetMAX30101(void);
static uint8_t BioSensors_ClearMAX30101Fifo(void);
static uint8_t BioSensors_ReadMAX30101Fifo(void);
static uint8_t BioSensors_ServicePpgInterrupt(void);
static void BioSensors_PushPpgSample(uint32_t red_sample, uint32_t ir_sample);
static uint16_t BioSensors_GetOrderedBufferIndex(uint16_t ordered_index);
static void BioSensors_UpdateVitalEstimates(void);

/**
 * @brief Returns the absolute value of a floating-point number.
 * @param value Input value.
 * @return Absolute value of @p value.
 */
static float BioSensors_AbsFloat(float value)
{
    return (value >= 0.0f) ? value : -value;
}

/**
 * @brief Constrains a floating-point value to a closed interval.
 * @param value Input value.
 * @param min_value Lower bound of the interval.
 * @param max_value Upper bound of the interval.
 * @return Clamped value within [@p min_value, @p max_value].
 */
static float BioSensors_ClampFloat(float value, float min_value, float max_value)
{
    if (value < min_value) {
        return min_value;
    }

    if (value > max_value) {
        return max_value;
    }

    return value;
}

/**
 * @brief Converts a floating-point reading to a scaled signed 16-bit integer.
 * @param value Floating-point reading to convert.
 * @return Rounded and saturated fixed-point value using BIOSENSORS_DATA_DECIMAL_SCALE.
 */
static int16_t BioSensors_FloatToScaledInt16(float value)
{
    float scaled_value = value * (float)BIOSENSORS_DATA_DECIMAL_SCALE;

    if (scaled_value > 32767.0f) {
        return 32767;
    }

    if (scaled_value < -32768.0f) {
        return (int16_t)-32768;
    }

    return (int16_t)((scaled_value >= 0.0f) ? (scaled_value + 0.5f) : (scaled_value - 0.5f));
}

/**
 * @brief Reads a single 8-bit register over I2C.
 * @param device_address 7-bit I2C device address.
 * @param register_address Register address to read.
 * @param value Output buffer that receives the register value.
 * @return BIOSENSORS_OK on success, otherwise BIOSENSORS_ERROR.
 */
static uint8_t BioSensors_ReadRegister8(uint8_t device_address, uint8_t register_address, uint8_t *value)
{
    if (value == 0) {
        return BIOSENSORS_ERROR;
    }

    if (HAL_I2C_Mem_Read(&hi2c3,
                         (uint16_t)(device_address << 1),
                         register_address,
                         I2C_MEMADD_SIZE_8BIT,
                         value,
                         1,
                         I2C_TIMEOUT) != HAL_OK) {
        return BIOSENSORS_ERROR;
    }

    return BIOSENSORS_OK;
}

/**
 * @brief Writes a single 8-bit register over I2C.
 * @param device_address 7-bit I2C device address.
 * @param register_address Register address to write.
 * @param value Register value to transmit.
 * @return BIOSENSORS_OK on success, otherwise BIOSENSORS_ERROR.
 */
static uint8_t BioSensors_WriteRegister8(uint8_t device_address, uint8_t register_address, uint8_t value)
{
    if (HAL_I2C_Mem_Write(&hi2c3,
                          (uint16_t)(device_address << 1),
                          register_address,
                          I2C_MEMADD_SIZE_8BIT,
                          &value,
                          1,
                          I2C_TIMEOUT) != HAL_OK) {
        return BIOSENSORS_ERROR;
    }

    return BIOSENSORS_OK;
}

/**
 * @brief Reads a sequence of bytes from a device register over I2C.
 * @param device_address 7-bit I2C device address.
 * @param register_address First register address to read.
 * @param buffer Output buffer for the received bytes.
 * @param length Number of bytes to read.
 * @return BIOSENSORS_OK on success, otherwise BIOSENSORS_ERROR.
 */
static uint8_t BioSensors_ReadBytes(uint8_t device_address, uint8_t register_address, uint8_t *buffer, uint16_t length)
{
    if ((buffer == 0) || (length == 0U)) {
        return BIOSENSORS_ERROR;
    }

    if (HAL_I2C_Mem_Read(&hi2c3,
                         (uint16_t)(device_address << 1),
                         register_address,
                         I2C_MEMADD_SIZE_8BIT,
                         buffer,
                         length,
                         I2C_TIMEOUT) != HAL_OK) {
        return BIOSENSORS_ERROR;
    }

    return BIOSENSORS_OK;
}

/**
 * @brief Issues a reset to the MAX30101 and waits for completion.
 * @return BIOSENSORS_OK if the reset bit clears before timeout, otherwise BIOSENSORS_ERROR.
 */
static uint8_t BioSensors_ResetMAX30101(void)
{
    uint8_t mode_config = 0U;
    uint8_t attempt;

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_MODE_CONFIG,
                                  MAX30101_MODE_RESET) != BIOSENSORS_OK) {
        return BIOSENSORS_ERROR;
    }

    HAL_Delay(10);

    for (attempt = 0U; attempt < 10U; ++attempt) {
        if (BioSensors_ReadRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                     MAX30101_REG_MODE_CONFIG,
                                     &mode_config) != BIOSENSORS_OK) {
            return BIOSENSORS_ERROR;
        }

        if ((mode_config & MAX30101_MODE_RESET) == 0U) {
            return BIOSENSORS_OK;
        }

        HAL_Delay(2);
    }

    return BIOSENSORS_ERROR;
}

/**
 * @brief Clears the MAX30101 FIFO state and pointer registers.
 * @return BIOSENSORS_OK on success, otherwise BIOSENSORS_ERROR.
 */
static uint8_t BioSensors_ClearMAX30101Fifo(void)
{
    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_FIFO_WR_PTR,
                                  0U) != BIOSENSORS_OK) {
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_OVF_COUNTER,
                                  0U) != BIOSENSORS_OK) {
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_FIFO_RD_PTR,
                                  0U) != BIOSENSORS_OK) {
        return BIOSENSORS_ERROR;
    }

    return BIOSENSORS_OK;
}

/**
 * @brief Stores one PPG sample pair into the circular buffers.
 * @param red_sample Raw red-channel sample from the MAX30101.
 * @param ir_sample Raw infrared-channel sample from the MAX30101.
 */
static void BioSensors_PushPpgSample(uint32_t red_sample, uint32_t ir_sample)
{
    g_bio_sensors.red_buffer[g_bio_sensors.write_index] = red_sample;
    g_bio_sensors.ir_buffer[g_bio_sensors.write_index] = ir_sample;

    g_bio_sensors.latest.red = red_sample;
    g_bio_sensors.latest.ir = ir_sample;

    g_bio_sensors.write_index = (uint16_t)((g_bio_sensors.write_index + 1U) % BIOSENSORS_PPG_BUFFER_SIZE);

    if (g_bio_sensors.sample_count < BIOSENSORS_PPG_BUFFER_SIZE) {
        g_bio_sensors.sample_count++;
    }
}

/**
 * @brief Maps a chronological sample index to the circular buffer index.
 * @param ordered_index Zero-based index in oldest-to-newest order.
 * @return Backing buffer index that contains the requested sample.
 */
static uint16_t BioSensors_GetOrderedBufferIndex(uint16_t ordered_index)
{
    uint16_t oldest_index = 0U;

    if (g_bio_sensors.sample_count >= BIOSENSORS_PPG_BUFFER_SIZE) {
        oldest_index = g_bio_sensors.write_index;
    }

    return (uint16_t)((oldest_index + ordered_index) % BIOSENSORS_PPG_BUFFER_SIZE);
}

/**
 * @brief Updates heart-rate and SpO2 estimates from buffered PPG samples.
 * @note This estimator is intentionally lightweight and not intended for clinical use.
 */
static void BioSensors_UpdateVitalEstimates(void)
{
    uint16_t index;
    uint16_t valid_sample_count = g_bio_sensors.sample_count;
    float red_sum = 0.0f;
    float ir_sum = 0.0f;
    float red_mean;
    float ir_mean;
    float red_ac_sum = 0.0f;
    float ir_ac_sum = 0.0f;
    float red_ac;
    float ir_ac;
    float ratio;
    float threshold;
    uint32_t ir_min = 0xFFFFFFFFUL;
    uint32_t ir_max = 0U;
    int32_t last_peak_index = -1;
    uint32_t last_peak_value = 0U;
    uint32_t interval_sum = 0U;
    uint16_t interval_count = 0U;
    uint16_t min_peak_distance;

    /* This is a lightweight polling estimator, not a clinical-grade algorithm. */
    g_bio_sensors.latest.heart_rate_valid = 0U;
    g_bio_sensors.latest.spo2_valid = 0U;

    if (valid_sample_count < BIOSENSORS_MIN_PPG_SAMPLES_FOR_ESTIMATION) {
        return;
    }

    for (index = 0U; index < valid_sample_count; ++index) {
        uint16_t buffer_index = BioSensors_GetOrderedBufferIndex(index);
        uint32_t red_sample = g_bio_sensors.red_buffer[buffer_index];
        uint32_t ir_sample = g_bio_sensors.ir_buffer[buffer_index];

        red_sum += (float)red_sample;
        ir_sum += (float)ir_sample;

        if (ir_sample > ir_max) {
            ir_max = ir_sample;
        }

        if (ir_sample < ir_min) {
            ir_min = ir_sample;
        }
    }

    red_mean = red_sum / (float)valid_sample_count;
    ir_mean = ir_sum / (float)valid_sample_count;

    if ((red_mean <= 1.0f) || (ir_mean <= 1.0f) || (ir_max <= ir_min)) {
        return;
    }

    for (index = 0U; index < valid_sample_count; ++index) {
        uint16_t buffer_index = BioSensors_GetOrderedBufferIndex(index);

        red_ac_sum += BioSensors_AbsFloat((float)g_bio_sensors.red_buffer[buffer_index] - red_mean);
        ir_ac_sum += BioSensors_AbsFloat((float)g_bio_sensors.ir_buffer[buffer_index] - ir_mean);
    }

    red_ac = red_ac_sum / (float)valid_sample_count;
    ir_ac = ir_ac_sum / (float)valid_sample_count;

    if ((red_ac > 0.0f) && (ir_ac > 0.0f)) {
        ratio = (red_ac / red_mean) / (ir_ac / ir_mean);
        g_bio_sensors.latest.spo2_percent = BioSensors_ClampFloat(110.0f - (25.0f * ratio),
                                                                  BIOSENSORS_SPO2_MIN_PERCENT,
                                                                  BIOSENSORS_SPO2_MAX_PERCENT);
        g_bio_sensors.latest.spo2_valid = 1U;
    }

    threshold = ir_mean + (((float)ir_max - ir_mean) * 0.35f);
    min_peak_distance = (uint16_t)((BIOSENSORS_PPG_EFFECTIVE_SAMPLE_RATE_HZ * 60U) / BIOSENSORS_MAX_HEART_RATE_BPM);

    if (min_peak_distance == 0U) {
        min_peak_distance = 1U;
    }

    for (index = 1U; index + 1U < valid_sample_count; ++index) {
        uint32_t previous_sample = g_bio_sensors.ir_buffer[BioSensors_GetOrderedBufferIndex((uint16_t)(index - 1U))];
        uint32_t current_sample = g_bio_sensors.ir_buffer[BioSensors_GetOrderedBufferIndex(index)];
        uint32_t next_sample = g_bio_sensors.ir_buffer[BioSensors_GetOrderedBufferIndex((uint16_t)(index + 1U))];

        if (((float)current_sample < threshold) ||
            (current_sample < previous_sample) ||
            (current_sample < next_sample)) {
            continue;
        }

        if ((last_peak_index >= 0) &&
            (((uint16_t)index - (uint16_t)last_peak_index) < min_peak_distance)) {
            if (current_sample > last_peak_value) {
                last_peak_index = (int32_t)index;
                last_peak_value = current_sample;
            }
            continue;
        }

        if (last_peak_index >= 0) {
            interval_sum += (uint32_t)((uint16_t)index - (uint16_t)last_peak_index);
            interval_count++;
        }

        last_peak_index = (int32_t)index;
        last_peak_value = current_sample;
    }

    if (interval_count > 0U) {
        float average_interval = (float)interval_sum / (float)interval_count;
        float heart_rate_bpm = (60.0f * (float)BIOSENSORS_PPG_EFFECTIVE_SAMPLE_RATE_HZ) / average_interval;

        if ((heart_rate_bpm >= (float)BIOSENSORS_MIN_HEART_RATE_BPM) &&
            (heart_rate_bpm <= (float)BIOSENSORS_MAX_HEART_RATE_BPM)) {
            g_bio_sensors.latest.heart_rate_bpm = heart_rate_bpm;
            g_bio_sensors.latest.heart_rate_valid = 1U;
        }
    }
}

/**
 * @brief Drains unread MAX30101 FIFO samples and refreshes vital estimates.
 * @return BIOSENSORS_OK if at least one new sample batch was processed, otherwise BIOSENSORS_ERROR.
 */
static uint8_t BioSensors_ReadMAX30101Fifo(void)
{
    uint8_t write_pointer = 0U;
    uint8_t read_pointer = 0U;
    uint8_t int_status_1 = 0U;
    uint8_t int_status_2 = 0U;
    uint8_t unread_samples;
    uint8_t fifo_bytes[BIOSENSORS_MAX_FIFO_BATCH_SAMPLES * MAX30101_FIFO_BYTES_PER_SPO2_SAMPLE];
    uint8_t did_update = 0U;

    if (g_bio_sensors.latest.max30101_available == 0U) {
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_ReadRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                 MAX30101_REG_INT_STATUS_1,
                                 &int_status_1) != BIOSENSORS_OK) {
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_ReadRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                 MAX30101_REG_INT_STATUS_2,
                                 &int_status_2) != BIOSENSORS_OK) {
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_ReadRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                 MAX30101_REG_FIFO_WR_PTR,
                                 &write_pointer) != BIOSENSORS_OK) {
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_ReadRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                 MAX30101_REG_FIFO_RD_PTR,
                                 &read_pointer) != BIOSENSORS_OK) {
        return BIOSENSORS_ERROR;
    }

    unread_samples = (uint8_t)((write_pointer - read_pointer) & MAX30101_FIFO_POINTER_MASK);

    /*
     * With rollover disabled and FIFO_A_FULL configured for 32 unread samples,
     * write and read pointers can match when the FIFO is full. In that case the
     * interrupt status is the disambiguator.
     */
    if ((unread_samples == 0U) &&
        ((int_status_1 & (MAX30101_INT_A_FULL | MAX30101_INT_ALC_OVF)) != 0U)) {
        unread_samples = MAX30101_FIFO_DEPTH_SAMPLES;
    }

    while (unread_samples > 0U) {
        uint8_t batch_samples = unread_samples;
        uint16_t byte_count;
        uint8_t sample_index;

        if (batch_samples > BIOSENSORS_MAX_FIFO_BATCH_SAMPLES) {
            batch_samples = BIOSENSORS_MAX_FIFO_BATCH_SAMPLES;
        }

        byte_count = (uint16_t)batch_samples * MAX30101_FIFO_BYTES_PER_SPO2_SAMPLE;

        if (BioSensors_ReadBytes(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                 MAX30101_REG_FIFO_DATA,
                                 fifo_bytes,
                                 byte_count) != BIOSENSORS_OK) {
            return BIOSENSORS_ERROR;
        }

        for (sample_index = 0U; sample_index < batch_samples; ++sample_index) {
            uint8_t *sample_bytes = &fifo_bytes[sample_index * MAX30101_FIFO_BYTES_PER_SPO2_SAMPLE];
            uint32_t red_sample = ((((uint32_t)sample_bytes[0]) << 16) |
                                   (((uint32_t)sample_bytes[1]) << 8) |
                                   ((uint32_t)sample_bytes[2])) & MAX30101_FIFO_DATA_MASK;
            uint32_t ir_sample = ((((uint32_t)sample_bytes[3]) << 16) |
                                  (((uint32_t)sample_bytes[4]) << 8) |
                                  ((uint32_t)sample_bytes[5])) & MAX30101_FIFO_DATA_MASK;

            BioSensors_PushPpgSample(red_sample, ir_sample);
        }

        unread_samples = (uint8_t)(unread_samples - batch_samples);
        did_update = BIOSENSORS_OK;
    }

    if (did_update == BIOSENSORS_OK) {
        BioSensors_UpdateVitalEstimates();
    }

    return did_update;
}

/**
 * @brief Services a pending PPG-ready event from the MAX30101 path.
 * @return BIOSENSORS_OK if any sensor data was updated, otherwise BIOSENSORS_ERROR.
 */
static uint8_t BioSensors_ServicePpgInterrupt(void)
{
    uint8_t updated = BIOSENSORS_ERROR;
    float unused_temperature = 0.0f;

    if (g_bio_sensors.ppg_ready_flag == 0U) {
        return BIOSENSORS_ERROR;
    }

    g_bio_sensors.ppg_ready_flag = 0U;

    if (BioSensors_ReadTemperature(&unused_temperature) == BIOSENSORS_OK) {
        updated = BIOSENSORS_OK;
    }

    if (BioSensors_ReadMAX30101Fifo() == BIOSENSORS_OK) {
        updated = BIOSENSORS_OK;
    }

    return updated;
}

/**
 * @brief Initializes the MAX30101 PPG sensor and clears runtime state.
 * @return BIOSENSORS_OK on success, otherwise BIOSENSORS_ERROR.
 */
uint8_t BioSensors_InitMAX30101(void)
{
    uint8_t part_id = 0U;
    uint8_t dummy_status = 0U;

    if (BioSensors_ReadRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                 MAX30101_REG_PART_ID,
                                 &part_id) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (part_id != MAX30101_EXPECTED_PART_ID) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_ResetMAX30101() != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_ClearMAX30101Fifo() != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_FIFO_CONFIG,
                                  BIOSENSORS_MAX30101_FIFO_CONFIG_VALUE) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_MODE_CONFIG,
                                  BIOSENSORS_MAX30101_MODE_VALUE) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_SPO2_CONFIG,
                                  BIOSENSORS_MAX30101_SPO2_CONFIG_VALUE) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_LED1_PA,
                                  BIOSENSORS_MAX30101_RED_LED_CURRENT) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_LED2_PA,
                                  BIOSENSORS_MAX30101_IR_LED_CURRENT) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_INT_ENABLE_1,
                                  BIOSENSORS_MAX30101_INTERRUPT_ENABLE_1) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                  MAX30101_REG_INT_ENABLE_2,
                                  0U) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30101_available = 0U;
        return BIOSENSORS_ERROR;
    }

    (void)BioSensors_ReadRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                   MAX30101_REG_INT_STATUS_1,
                                   &dummy_status);
    (void)BioSensors_ReadRegister8(BIOSENSORS_MAX30101_ADDRESS_7BIT,
                                   MAX30101_REG_INT_STATUS_2,
                                   &dummy_status);

    g_bio_sensors.latest.max30101_available = 1U;
    g_bio_sensors.latest.heart_rate_valid = 0U;
    g_bio_sensors.latest.spo2_valid = 0U;
    g_bio_sensors.latest.red = 0U;
    g_bio_sensors.latest.ir = 0U;
    g_bio_sensors.sample_count = 0U;
    g_bio_sensors.write_index = 0U;
    g_bio_sensors.ppg_ready_flag = 0U;

    return BIOSENSORS_OK;
}

/**
 * @brief Initializes the MAX30205 temperature sensor.
 * @return BIOSENSORS_OK on success, otherwise BIOSENSORS_ERROR.
 */
uint8_t BioSensors_InitMAX30205(void)
{
    uint8_t configuration = 0U;

    configuration = MAX30205_BuildConfig(0U,
                                         0U,
                                         0U,
                                         MAX30205_CONFIG_FAULT_QUEUE_1,
                                         0U,
                                         0U,
                                         0U);

    if (BioSensors_WriteRegister8(BIOSENSORS_MAX30205_ADDRESS_7BIT,
                                  MAX30205_REG_CONFIGURATION,
                                  configuration) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30205_available = 0U;
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_ReadRegister8(BIOSENSORS_MAX30205_ADDRESS_7BIT,
                                 MAX30205_REG_CONFIGURATION,
                                 &configuration) != BIOSENSORS_OK) {
        g_bio_sensors.latest.max30205_available = 0U;
        return BIOSENSORS_ERROR;
    }

    g_bio_sensors.latest.max30205_available = 1U;
    g_bio_sensors.latest.temperature_valid = 0U;

    return BIOSENSORS_OK;
}

/**
 * @brief Initializes all supported biosensors on the shared I2C bus.
 * @return BIOSENSORS_OK only if both MAX30101 and MAX30205 initialization succeed.
 */
uint8_t BioSensors_Init(void)
{
    uint8_t max30101_status = BioSensors_InitMAX30101();
    uint8_t max30205_status = BioSensors_InitMAX30205();

    return (uint8_t)((max30101_status == BIOSENSORS_OK) && (max30205_status == BIOSENSORS_OK));
}

/**
 * @brief Reads the current temperature from the MAX30205.
 * @param temperature_c Output pointer that receives the temperature in degrees Celsius.
 * @return BIOSENSORS_OK on success, otherwise BIOSENSORS_ERROR.
 */
static uint8_t BioSensors_ReadTemperature(float *temperature_c)
{
    uint8_t raw_temperature[MAX30205_TEMPERATURE_REGISTER_SIZE_BYTES] = {0U, 0U};
    float temperature_value;

    if ((temperature_c == 0) || (g_bio_sensors.latest.max30205_available == 0U)) {
        return BIOSENSORS_ERROR;
    }

    if (BioSensors_ReadBytes(BIOSENSORS_MAX30205_ADDRESS_7BIT,
                             MAX30205_REG_TEMPERATURE,
                             raw_temperature,
                             MAX30205_TEMPERATURE_REGISTER_SIZE_BYTES) != BIOSENSORS_OK) {
        g_bio_sensors.latest.temperature_valid = 0U;
        return BIOSENSORS_ERROR;
    }

    temperature_value = MAX30205_BytesToCelsius(raw_temperature[0], raw_temperature[1], 0U);

    g_bio_sensors.latest.temperature_c = temperature_value;
    g_bio_sensors.latest.temperature_valid = 1U;
    *temperature_c = temperature_value;

    return BIOSENSORS_OK;
}

/**
 * @brief Updates cached biosensor readings when new data is available.
 * @return BIOSENSORS_OK if any cached reading was refreshed, otherwise BIOSENSORS_ERROR.
 */
uint8_t BioSensors_Update(void)
{
    uint8_t updated = BIOSENSORS_ERROR;
    float unused_temperature = 0.0f;

    if (g_bio_sensors.ppg_ready_flag != 0U) {
        return BioSensors_ServicePpgInterrupt();
    }

    if ((g_bio_sensors.latest.temperature_valid == 0U) &&
        (BioSensors_ReadTemperature(&unused_temperature) == BIOSENSORS_OK)) {
        updated = BIOSENSORS_OK;
    }

    return updated;
}

/**
 * @brief Packs the latest scaled biosensor readings into a byte buffer.
 * @param data Output buffer that receives heart rate, SpO2, and temperature as big-endian int16 values.
 * @return BIOSENSORS_OK when all three readings are valid, otherwise BIOSENSORS_ERROR.
 */
uint8_t BioSensors_ReadData(uint8_t* data)
{
    if (data == 0) {
        return BIOSENSORS_ERROR;
    }

    // (void)BioSensors_Update();

    int16_t sensor_data[BIOSENSORS_DATA_VALUE_COUNT];
    sensor_data[BIOSENSORS_DATA_INDEX_HEART_RATE] =
        (g_bio_sensors.latest.heart_rate_valid != 0U) ?
            BioSensors_FloatToScaledInt16(g_bio_sensors.latest.heart_rate_bpm) : 0;
    sensor_data[BIOSENSORS_DATA_INDEX_SPO2] =
        (g_bio_sensors.latest.spo2_valid != 0U) ?
            BioSensors_FloatToScaledInt16(g_bio_sensors.latest.spo2_percent) : 0;
    sensor_data[BIOSENSORS_DATA_INDEX_TEMPERATURE] =
        (g_bio_sensors.latest.temperature_valid != 0U) ?
            BioSensors_FloatToScaledInt16(g_bio_sensors.latest.temperature_c) : 0;

    data[0] = (sensor_data[BIOSENSORS_DATA_INDEX_HEART_RATE] & 0xff00) >> 8;
    data[1] = (sensor_data[BIOSENSORS_DATA_INDEX_HEART_RATE] & 0x00ff);
    data[2] = (sensor_data[BIOSENSORS_DATA_INDEX_SPO2] & 0xff00) >> 8;
    data[3] = (sensor_data[BIOSENSORS_DATA_INDEX_SPO2] & 0x00ff);
    data[4] = (sensor_data[BIOSENSORS_DATA_INDEX_TEMPERATURE] & 0xff00) >> 8;
    data[5] = (sensor_data[BIOSENSORS_DATA_INDEX_TEMPERATURE] & 0x00ff);
    
    if ((g_bio_sensors.latest.heart_rate_valid != 0U) &&
        (g_bio_sensors.latest.spo2_valid != 0U) &&
        (g_bio_sensors.latest.temperature_valid != 0U)) {
        return BIOSENSORS_OK;
    }

    return BIOSENSORS_ERROR;
}

/**
 * @brief Signals that a new batch of PPG samples is ready to be serviced.
 */
void BioSensors_NotifyPpgReady(void)
{
    g_bio_sensors.ppg_ready_flag = 1U;
}
