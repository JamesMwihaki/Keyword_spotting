#include "Speaker.h"

void Speaker::begin() {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 16000,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo (stable)
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,      // Increased from 4 for smoother playback
      .dma_buf_len = 512,
      .use_apll = false,
      .tx_desc_auto_clear = true};

  i2s_pin_config_t pin_config = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = SPEAKER_BCLK,
      .ws_io_num = SPEAKER_LRC,
      .data_out_num = SPEAKER_DIN,
      .data_in_num = I2S_PIN_NO_CHANGE};

  esp_err_t err = i2s_driver_install(SPEAKER_I2S_NUM, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("Failed to install Speaker I2S driver: %d\n", err);
  } else {
    Serial.println("Speaker I2S Driver Installed");
  }

  err = i2s_set_pin(SPEAKER_I2S_NUM, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("Failed to set Speaker I2S pins: %d\n", err);
  } else {
    Serial.println("Speaker I2S Pins Configured");
  }

  i2s_zero_dma_buffer(SPEAKER_I2S_NUM);
}

void Speaker::test() {
  Serial.println("Testing Speaker...");

  const size_t CHUNK_FRAMES = 256; // frames (stereo = 2 samples per frame)
  int16_t buffer[CHUNK_FRAMES * 2]; // stereo interleaved

  const int sample_rate = 16000;
  const int frequency = 440;
  const int duration_ms = 500;
  const int total_frames = (sample_rate * duration_ms) / 1000;

  int generated = 0;
  while (generated < total_frames) {
    int frames = CHUNK_FRAMES;
    if (generated + frames > total_frames)
      frames = total_frames - generated;

    for (int i = 0; i < frames; i++) {
      int16_t sample = (int16_t)(10000 * sin(2.0 * PI * (generated + i) *
                                             frequency / sample_rate));
      buffer[i * 2]     = sample; // Left
      buffer[i * 2 + 1] = sample; // Right
    }

    size_t bytes_written;
    i2s_write(SPEAKER_I2S_NUM, buffer, frames * 4, &bytes_written,
              1000 / portTICK_PERIOD_MS);
    generated += frames;
    vTaskDelay(1);
  }

  Serial.println("Speaker Test Complete");
  i2s_zero_dma_buffer(SPEAKER_I2S_NUM);
  delay(100);
}

void Speaker::playChunk(const uint8_t *data, size_t size) {
  // Input: Stereo 16-bit (4 bytes per frame) from Backend
  // Output: I2S Stereo (4 bytes per frame)
  // Direct pass-through - no processing needed!

  size_t bytes_written;
  // Use a timeout (e.g. 1000ms) instead of portMAX_DELAY to prevent WDT resets
  esp_err_t err = i2s_write(SPEAKER_I2S_NUM, data, size, &bytes_written,
                            1000 / portTICK_PERIOD_MS);
  if (err != ESP_OK) {
    Serial.printf("I2S Write Failed or Timeout: %d\n", err);
  } else if (bytes_written != size) {
    Serial.printf("I2S Write Review: Written %d / %d bytes\n", bytes_written,
                  size);
  }
}

void Speaker::stop() {
  i2s_zero_dma_buffer(SPEAKER_I2S_NUM);
}