#include "mic.h"
#include "MotorControl.h"
#include <JamesMwihaki-project-1_inferencing.h>
#include "driver/i2s.h"

typedef struct {
    signed short *buffers[2];
    unsigned char buf_select;
    unsigned char buf_ready;
    unsigned int buf_count;
    unsigned int n_samples;
} inference_t;

static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static int32_t raw_samples[sample_buffer_size];
static bool record_status = true;

// Internal Helpers
static int i2s_init(uint32_t sampling_rate);
static bool microphone_inference_start(uint32_t n_samples);
static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
static void capture_samples(void* arg);

void setupVoice() {
    run_classifier_init();
    if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
        Serial.println("ERR: Failed to setup Voice Control");
        return;
    }
    Serial.println("Voice Recognition Active");
}

void checkVoice() {
    if (inference.buf_ready == 0) return;
    
    inference.buf_ready = 0;
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;
    
    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, false);
    
    if (r != EI_IMPULSE_OK) return;

    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
        if (result.classification[ix].value > CONFIDENCE_THRESHOLD) {
            Serial.print(result.classification[ix].label);
            Serial.print(": ");
            Serial.println(result.classification[ix].value);
            

            //logic for motor control
            const char* label = result.classification[ix].label;
            if (strcmp(label, "raise_green_screen") == 0) {
                start_motor_sequence(0, "UP"); 
            } else if (strcmp(label, "drop_green_screen") == 0) {
                start_motor_sequence(0, "DOWN");
            }
        }
    }
}

void start_motor_sequence(int index, const char* cmd) {
    if (index >= 0 && index < numMotors && !isEmergencyStopActive) {
        moveMotor(motors[index].id, cmd);
        motors[index].startTime = millis();
        motors[index].isRunning = true;
    }
}

static void audio_inference_callback(uint32_t n_samples) {
    for(int i = 0; i < n_samples; i++) {
        inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];
        if(inference.buf_count >= inference.n_samples) {
            inference.buf_select ^= 1;
            inference.buf_count = 0;
            inference.buf_ready = 1;
        }
    }
}

static void capture_samples(void* arg) {
    size_t bytes_read = 0;
    while (record_status) {
        esp_err_t result = i2s_read(I2S_NUM_0, &raw_samples, sizeof(raw_samples), &bytes_read, portMAX_DELAY);
        if (result == ESP_OK && bytes_read > 0) {
            Serial.write((uint8_t *)raw_samples, bytes_read); //Bridge between the python and serial
            int samplesCount = bytes_read / 4;
            for (int i = 0; i < samplesCount; i++) {
                sampleBuffer[i] = (int16_t)(raw_samples[i] >> 16);
            }
            audio_inference_callback(samplesCount);
        }
    }
    vTaskDelete(NULL);
}

static bool microphone_inference_start(uint32_t n_samples) {
    inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));
    inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));
    if (inference.buffers[0] == NULL || inference.buffers[1] == NULL) return false;

    inference.buf_select = 0;
    inference.buf_count = 0;
    inference.n_samples = n_samples;
    inference.buf_ready = 0;

    if (i2s_init(EI_CLASSIFIER_FREQUENCY) != 0) return false;

    record_status = true;
    xTaskCreate(capture_samples, "CaptureSamples", 1024 * 8, NULL, 10, NULL);
    return true;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
    numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset], out_ptr, length);
    return 0;
}

static int i2s_init(uint32_t sampling_rate) {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = sampling_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false
    };
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = 32,    // I2S Clock
        .ws_io_num = 25,     // I2S Word Select
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = 33    // I2S Data In
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    return i2s_set_pin(I2S_NUM_0, &pin_config);
}
