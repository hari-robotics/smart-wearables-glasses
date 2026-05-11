# Smart Wearables Team B3

This repo contains both the STM32 firmware and the android application, if you are new to git, here's the [quick start for using git in vscode](https://code.visualstudio.com/docs/sourcecontrol/quickstart).

## Current Workflow
Current workflows are:

1. Initializing sensors
    ```cpp
    (void)BioSensors_Init();
    ```

2. Trying to update the sensor data
    ```cpp
    while (1) {
        ...
        (void)BioSensors_Update();
        ...
    }
    ```

    In `BioSensors_Update` function, it checks the flag `g_bio_sensors.ppg_ready_flag`, which is updated in function

    ```cpp
    void BioSensors_NotifyPpgReady(void)
    ```

    And this function is called in 
    ```cpp
    void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
        ...
        BioSensors_NotifyPpgReady();
        ...
    }
    ```

    This function is controlled by external GPIO interrupt by MAX30101 interrupt PIN.

3. In function 
    ```cpp
    uint8_t BioSensors_ServicePpgInterrupt(void) 
    ```
    It reads the temperature sensor data and PPG sensor data into the sensor buffers `g_bio_sensors.red_buffer[g_bio_sensors.write_index]` and `g_bio_sensors.ir_buffer[g_bio_sensors.write_index]`, and save the latest data to `g_bio_sensors.latest.red` and `g_bio_sensors.latest.ir`

    After updating the data, it calls
    ```cpp
    BioSensors_UpdateVitalEstimates();
    ```
    to estimate the HR and SpO2 data and saves them into `g_bio_sensors.latest.heart_rate_bpm` and `g_bio_sensors.latest.spo2_percent`.

4. The estimated data were send in each external interruption
    ```cpp
    void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
        ...
        // Acquire byte stream data of BIO sensors
        uint8_t bio_data[6];
        BioSensors_ReadData(bio_data);

        // Send BIO sensor data through BLE (HR, SpO2, Temp)
        BLE_SendPacket(DATA_TYPE_BIO_SENSORS, bio_data);
        ...
    }
    ```


## TODOs

- [ ] Write the data to SPI flash instead store them in RAM

- [ ] Low power applications

- [ ] Improve `BioSensors_UpdateVitalEstimates()` for better estimation

- [ ] FSM design for wearing detection and different working scneraios

- [ ] Change data reading logic from blocking to DMA with interruption to improve performance and energy cost behavior

- [ ] (Optional) Integrate neual networks into the APP to get a better data estimation