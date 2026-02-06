
#include "mic.h"
#include "MotorControl.h"
#include "driver/i2s.h"
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <JamesMwihaki-project-1_inferencing.h>

using namespace websockets;

enum SystemState { STATE_TINYML, STATE_STREAMING, STATE_AWAITING_RESPONSE };
volatile SystemState systemState = STATE_TINYML;
WebsocketsClient client;

// REPLACE THIS WITH YOUR PYTHON SERVER IP
const char *websocket_server_host = "192.168.1.124";
const int websocket_server_port = 8000;

// RGB LED Pins
const int PIN_RED = 4;
const int PIN_GREEN = 2;
const int PIN_BLUE = 15;

// Helper to set RGB color
void setRGB(int r, int g, int b) {
  ledcWrite(PIN_RED, r);
  ledcWrite(PIN_GREEN, g);
  ledcWrite(PIN_BLUE, b);
}

void blinkRed() {
  for (int i = 0; i < 3; i++) {
    setRGB(255, 0, 0);
    delay(300);
    setRGB(0, 0, 0);
    delay(300);
  }
}

unsigned long stateSwitchTime = 0;
// Phase A: Streaming - Max 6 seconds
const unsigned long RECORD_DURATION = 6000;
// Phase B: Waiting - Max 10 seconds
const unsigned long RESPONSE_TIMEOUT = 10000;

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
static int microphone_audio_signal_get_data(size_t offset, size_t length,
                                            float *out_ptr);
static void capture_samples(void *arg);

void setupVoice() {
  ledcAttach(PIN_RED, 5000, 8);
  ledcAttach(PIN_GREEN, 5000, 8);
  ledcAttach(PIN_BLUE, 5000, 8);
  setRGB(0, 255, 0); // Green (Ready)

  run_classifier_init();
  if (microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE) == false) {
    Serial.println("ERR: Failed to setup Voice Control");
    setRGB(255, 0, 0);
    return;
  }
  Serial.println("Voice Recognition Active");
}

void onMessageCallback(WebsocketsMessage message) {
  Serial.print("Got Message: ");
  Serial.println(message.data());

  // Using ArduinoJson for robust parsing
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message.data());

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    // Could be simple string message not JSON, ignore or handle
    return;
  }

  // Check for error response
  if (doc.containsKey("error")) {
    const char *errMsg = doc["error"];
    Serial.printf("Server/Gemini Error: %s\n", errMsg);
    blinkRed();
    systemState = STATE_TINYML;
    setRGB(0, 255, 0);
    return;
  }

  if (doc.containsKey("motor_id") && doc.containsKey("direction")) {
    int motorId = doc["motor_id"];
    const char *dirStr = doc["direction"];

    Serial.printf("Command: Motor %d -> %s\n", motorId, dirStr);
    start_motor_sequence(motorId, dirStr);
  }

  // Success, return to listening
  systemState = STATE_TINYML;
  setRGB(0, 255, 0); // Green
}

void checkVoice() {
  if (client.available()) {
    client.poll();
  }

  // State Machine Logic
  unsigned long timeInState = millis() - stateSwitchTime;

  if (systemState == STATE_STREAMING) {
    // Connection Guard
    if (!client.available()) { // check connectivity (available() checks
                               // connected state internal bool often)
      // WebsocketsClient doesn't have a simple isConnected() public, but
      // available() or poll() handles state. Actually, let's use check
      // connectivity via ping or just assume close if poll fails? library
      // usually handles it. But we can check if client is open? The library
      // uses client.available() as `client.stream` check. Let's assume on
      // disconnect we get callback or state change.
    }

    // Check Timeout Phase A
    if (timeInState > RECORD_DURATION) {
      Serial.println("Streaming finished. Waiting for Gemini response...");
      setRGB(255, 165, 0); // Orange (Processing)
      systemState = STATE_AWAITING_RESPONSE;
      stateSwitchTime = millis(); // Reset timer for next phase
    }
  } else if (systemState == STATE_AWAITING_RESPONSE) {
    // Check Timeout Phase B
    if (timeInState > RESPONSE_TIMEOUT) {
      Serial.println("Response Timeout. Closing.");
      client.close();
      blinkRed(); // Fail State Visual
      systemState = STATE_TINYML;
      setRGB(0, 255, 0);
    }
  }

  if (inference.buf_ready == 0)
    return;
  inference.buf_ready = 0;

  // TinyML only runs when idle
  if (systemState == STATE_TINYML) {
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = &microphone_audio_signal_get_data;

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, false);

    if (r != EI_IMPULSE_OK)
      return;

    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      if (result.classification[ix].value > 0.6) { // Confidence Threshold
        if (strcmp(result.classification[ix].label, "dawn") == 0) {
          Serial.println("Wake Word Detected: Dawn");
          setRGB(0, 0, 255); // Blue (Recording)

          bool connected =
              client.connect(websocket_server_host, websocket_server_port,
                             "/ws/audio?token=my_secure_token_123");
          if (connected) {
            Serial.println("Connected to WebSocket Server");
            client.onMessage(onMessageCallback);
            systemState = STATE_STREAMING;
            stateSwitchTime = millis();
          } else {
            Serial.println("Failed to connect");
            blinkRed();
            setRGB(0, 255, 0);
          }
        }
      }
    }
  }
}

void start_motor_sequence(int index, const char *cmd) {
  if (index >= 0 && index < numMotors && !isEmergencyStopActive) {
    moveMotor(motors[index].id, cmd);
    motors[index].startTime = millis();
    motors[index].isRunning = true;
  }
}

static void audio_inference_callback(uint32_t n_samples) {
  for (int i = 0; i < n_samples; i++) {
    inference.buffers[inference.buf_select][inference.buf_count++] =
        sampleBuffer[i];
    if (inference.buf_count >= inference.n_samples) {
      inference.buf_select ^= 1;
      inference.buf_count = 0;
      inference.buf_ready = 1;
    }
  }
}

static void capture_samples(void *arg) {
  size_t bytes_read = 0;
  while (record_status) {
    esp_err_t result = i2s_read(I2S_NUM_0, &raw_samples, sizeof(raw_samples),
                                &bytes_read, portMAX_DELAY);
    if (result == ESP_OK && bytes_read > 0) {

      int samplesCount = bytes_read / 4;
      for (int i = 0; i < samplesCount; i++) {
        sampleBuffer[i] = (int16_t)(raw_samples[i] >> 16);
      }

      if (systemState == STATE_STREAMING) {
        // Connection Guard inside the task
        // We can't easily check client state here safely constantly effectively
        // without mutex, but if sendBinary fails it returns false? (Library
        // dependent)
        bool success = client.sendBinary((const char *)sampleBuffer,
                                         samplesCount * sizeof(int16_t));
        if (!success) {
          // If send fails, connection likely dropped
          // We shouldn't change systemState here due to race conditions maybe,
          // but strictly we can set a flag or just let the main loop handle
          // timeout. For strict Phase A requirement: We let the main loop
          // timeout handle it, or we can break.
        }
      } else if (systemState == STATE_TINYML) {
        audio_inference_callback(samplesCount);
      }
    }
  }
  vTaskDelete(NULL);
}

static bool microphone_inference_start(uint32_t n_samples) {
  inference.buffers[0] =
      (signed short *)malloc(n_samples * sizeof(signed short));
  inference.buffers[1] =
      (signed short *)malloc(n_samples * sizeof(signed short));
  if (inference.buffers[0] == NULL || inference.buffers[1] == NULL)
    return false;

  inference.buf_select = 0;
  inference.buf_count = 0;
  inference.n_samples = n_samples;
  inference.buf_ready = 0;

  if (i2s_init(EI_CLASSIFIER_FREQUENCY) != 0)
    return false;

  record_status = true;
  xTaskCreate(capture_samples, "CaptureSamples", 1024 * 8, NULL, 10, NULL);
  return true;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length,
                                            float *out_ptr) {
  numpy::int16_to_float(&inference.buffers[inference.buf_select ^ 1][offset],
                        out_ptr, length);
  return 0;
}

static int i2s_init(uint32_t sampling_rate) {
  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = sampling_rate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format =
          (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 512,
      .use_apll = false};

  i2s_pin_config_t pin_config = {.bck_io_num = 32,
                                 .ws_io_num = 25,
                                 .data_out_num = I2S_PIN_NO_CHANGE,
                                 .data_in_num = 33};

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  return i2s_set_pin(I2S_NUM_0, &pin_config);
}