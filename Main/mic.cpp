#include "mic.h"
#include "MotorControl.h"
#include "Speaker.h"
#include "WebSocketManager.h"
#include "driver/i2s.h"
#include <ArduinoJson.h>
#include <JamesMwihaki-project-1_inferencing.h>

enum SystemState {
  STATE_TINYML,
  STATE_STREAMING,
  STATE_AWAITING_RESPONSE,
  STATE_PLAYBACK
};
volatile SystemState systemState = STATE_TINYML;
volatile bool shouldRetryListening = false;
volatile int retryCount = 0;
const int MAX_RETRIES = 3;

// Flags set by Core 1 (checkVoice), executed by Core 0 (capture task)
// to keep all WebSocket operations on a single core.
static volatile bool wsSendEndOfAudio = false;

WebSocketManager ws;
Speaker speaker;

const char *websocket_server_host = "192.168.1.124";
const int websocket_server_port = 8000;

// Individual LED Pins
const int PIN_RED    = 4;
const int PIN_GREEN  = 2;
const int PIN_BLUE   = 15;
const int PIN_YELLOW = 12; // Playback state
const int PIN_WHITE  = 13; // Awaiting response

void setAllLEDs(bool red, bool green, bool blue, bool yellow, bool white) {
  digitalWrite(PIN_RED,    red    ? HIGH : LOW);
  digitalWrite(PIN_GREEN,  green  ? HIGH : LOW);
  digitalWrite(PIN_BLUE,   blue   ? HIGH : LOW);
  digitalWrite(PIN_YELLOW, yellow ? HIGH : LOW);
  digitalWrite(PIN_WHITE,  white  ? HIGH : LOW);
}

void blinkRed() {
  for (int i = 0; i < 3; i++) {
    setAllLEDs(true, false, false, false, false);
    delay(300);
    setAllLEDs(false, false, false, false, false);
    delay(300);
  }
}

unsigned long stateSwitchTime = 0;
unsigned long lastAudioTime   = 0;
const unsigned long RECORD_DURATION  = 3000;
const unsigned long RESPONSE_TIMEOUT = 20000;
const unsigned long PLAYBACK_TIMEOUT = 2000;

typedef struct {
  signed short *buffers[2];
  unsigned char buf_select;
  unsigned char buf_ready;
  unsigned int  buf_count;
  unsigned int  n_samples;
} inference_t;

static inference_t inference;
static const uint32_t sample_buffer_size = 2048;
static signed short sampleBuffer[sample_buffer_size];
static int32_t      raw_samples[sample_buffer_size];
static bool record_status = true;

static int  i2s_init(uint32_t sampling_rate);
static bool microphone_inference_start(uint32_t n_samples);
static int  microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr);
static void capture_samples(void *arg);

void setupVoice() {
  pinMode(PIN_RED,    OUTPUT);
  pinMode(PIN_GREEN,  OUTPUT);
  pinMode(PIN_BLUE,   OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_WHITE,  OUTPUT);

  // LED startup test
  Serial.println("Testing LEDs...");
  setAllLEDs(true, false, false, false, false);  delay(200);
  setAllLEDs(false, true, false, false, false);  delay(200);
  setAllLEDs(false, false, true, false, false);  delay(200);
  setAllLEDs(false, false, false, true, false);  delay(200);
  setAllLEDs(false, false, false, false, true);  delay(200);
  setAllLEDs(false, true, false, false, false);  // Green = ready

  speaker.begin();
  speaker.test();
  delay(500);

  ws.begin(websocket_server_host, websocket_server_port,
           "/ws/audio?token=my_secure_token_123");

  // Binary callback: incoming audio from server → play it
  ws.onBinary([](const uint8_t *data, size_t len) {
    // Ignore audio while streaming (would kill the recording window)
    // or while listening for wake word (late chunks must not interrupt TinyML).
    if (systemState == STATE_STREAMING) return;
    if (systemState == STATE_TINYML) return;

    if (systemState != STATE_PLAYBACK) {
      Serial.println("Audio received. Switching to PLAYBACK...");
      setAllLEDs(false, false, false, true, false); // Yellow
      systemState = STATE_PLAYBACK;
    }
    speaker.playChunk(data, len);
    lastAudioTime = millis();
  });

  // Text callback: JSON events from backend
  ws.onText([](const char *msg) {
    Serial.printf("Got Text: %s\n", msg);

    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, msg);
    if (!error && doc.containsKey("event")) {
      const char *event = doc["event"];
      if (strcmp(event, "retry_listening") == 0) {
        Serial.println("EVENT: retry_listening");
        shouldRetryListening = true;
      } else if (strcmp(event, "command_fulfilled") == 0) {
        Serial.println("EVENT: command_fulfilled");
        shouldRetryListening = false;
      }
    }
  });

  run_classifier_init();
  if (!microphone_inference_start(EI_CLASSIFIER_SLICE_SIZE)) {
    Serial.println("ERR: Failed to setup Voice Control");
    setAllLEDs(true, false, false, false, false); // Red = error
    return;
  }
  Serial.println("Voice Recognition Active");
}

static bool connectWithBackoff() {
  const int MAX_ATTEMPTS   = 4;
  const int BACKOFF_BASE_MS = 500; // delays: 500ms, 1s, 2s
  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    Serial.printf("WS connect attempt %d/%d...\n", attempt, MAX_ATTEMPTS);
    if (ws.connect()) {
      Serial.println("WS connected.");
      return true;
    }
    if (attempt < MAX_ATTEMPTS) {
      int waitMs = BACKOFF_BASE_MS * (1 << (attempt - 1));
      Serial.printf("Connect failed. Retrying in %dms...\n", waitMs);
      setAllLEDs(true, false, false, false, false); // Red = retrying
      delay(waitMs);
      setAllLEDs(false, false, true, false, false); // Blue = still trying
    }
  }
  return false;
}

void checkVoice() {
  // ws.update() is called from capture_samples task (Core 0) to avoid
  // concurrent access to the WebSocket client from two cores.

  unsigned long timeInState = millis() - stateSwitchTime;

  if (systemState == STATE_STREAMING) {
    if (!ws.isConnected()) {
      Serial.println("WS disconnected during streaming");
      systemState = STATE_TINYML;
      setAllLEDs(false, true, false, false, false);
      return;
    }
    if (timeInState > RECORD_DURATION) {
      Serial.println("Streaming done. Waiting for response...");
      wsSendEndOfAudio = true; // Core 0 (capture task) will send this
      setAllLEDs(true, false, false, false, false); // Red = processing
      systemState = STATE_AWAITING_RESPONSE;
      stateSwitchTime = millis();
    }

  } else if (systemState == STATE_AWAITING_RESPONSE) {
    if (timeInState > RESPONSE_TIMEOUT) {
      Serial.println("Response timeout. Resetting.");
      ws.close(); // Clean up — don't leave a dangling connection
      blinkRed();
      systemState = STATE_TINYML;
      setAllLEDs(false, true, false, false, false);
    }

  } else if (systemState == STATE_PLAYBACK) {
    if (millis() - lastAudioTime > PLAYBACK_TIMEOUT) {
      speaker.stop();

      if (shouldRetryListening && retryCount < MAX_RETRIES) {
        retryCount++;
        Serial.printf("Retrying listen (%d/%d)...\n", retryCount, MAX_RETRIES);
        setAllLEDs(false, false, true, false, false); // Blue = recording
        systemState = STATE_STREAMING;
        stateSwitchTime = millis();
        shouldRetryListening = false;
      } else {
        if (retryCount >= MAX_RETRIES)
          Serial.println("Max retries reached. Returning to wake word.");
        else
          Serial.println("Command fulfilled. Returning to wake word.");
        retryCount = 0;
        shouldRetryListening = false;
        ws.close(); // Close connection; next wake word opens a fresh session
        systemState = STATE_TINYML;
        setAllLEDs(false, true, false, false, false); // Green
      }
    }
  }

  if (inference.buf_ready == 0)
    return;
  inference.buf_ready = 0;

  if (systemState == STATE_TINYML) {
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data     = &microphone_audio_signal_get_data;

    ei_impulse_result_t result = {0};
    EI_IMPULSE_ERROR r = run_classifier_continuous(&signal, &result, false);
    if (r != EI_IMPULSE_OK)
      return;

    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      if (result.classification[ix].value > CONFIDENCE_THRESHOLD) {
        if (strcmp(result.classification[ix].label, "dawn") == 0) {
          Serial.println("Wake word detected: Dawn");
          setAllLEDs(false, false, true, false, false); // Blue

          if (ws.isConnected() || connectWithBackoff()) {
            Serial.println("WebSocket ready. Starting stream...");
            setAllLEDs(false, false, true, false, false); // Blue = recording
            systemState = STATE_STREAMING;
            stateSwitchTime = millis();
          } else {
            Serial.println("All connection attempts failed.");
            blinkRed();
            setAllLEDs(false, true, false, false, false); // Green = back to wake word
          }
        }
      }
    }
  }
}

static void audio_inference_callback(uint32_t n_samples) {
  for (uint32_t i = 0; i < n_samples; i++) {
    inference.buffers[inference.buf_select][inference.buf_count++] = sampleBuffer[i];
    if (inference.buf_count >= inference.n_samples) {
      inference.buf_select ^= 1;
      inference.buf_count   = 0;
      inference.buf_ready   = 1;
    }
  }
}

static void capture_samples(void *arg) {
  size_t bytes_read = 0;
  while (record_status) {
    esp_err_t result = i2s_read(I2S_NUM_0, &raw_samples, sizeof(raw_samples),
                                &bytes_read, pdMS_TO_TICKS(100));
    if (result == ESP_OK && bytes_read > 0) {
      int samplesCount = bytes_read / 4;
      for (int i = 0; i < samplesCount; i++) {
        // Shift 32-bit I2S value down to 16-bit signed PCM
        sampleBuffer[i] = (int16_t)(raw_samples[i] >> 16);
      }

      if (systemState == STATE_STREAMING) {
        if (ws.isConnected()) {
          // Send in small 512-byte chunks to avoid large heap allocations
          // inside the WebSocket library's internal frame builder.
          const size_t CHUNK = 256; // int16 samples per frame = 512 bytes
          for (int offset = 0; offset < samplesCount; offset += CHUNK) {
            size_t batch = ((size_t)(samplesCount - offset) < CHUNK)
                               ? (size_t)(samplesCount - offset)
                               : CHUNK;
            ws.sendBinary((const uint8_t *)(sampleBuffer + offset),
                          batch * sizeof(int16_t));
          }
        } else {
          Serial.println("WS lost during streaming. Resetting.");
          systemState = STATE_TINYML;
          setAllLEDs(false, true, false, false, false);
        }
      } else if (systemState == STATE_TINYML) {
        audio_inference_callback(samplesCount);
      }
    }
    // All WebSocket operations (send + receive) run here on Core 0
    // to prevent concurrent access with Core 1 (main loop).
    if (wsSendEndOfAudio) {
      wsSendEndOfAudio = false;
      if (ws.isConnected()) {
        ws.sendText("{\"event\": \"end_of_audio\"}");
      }
    }
    ws.update();
    vTaskDelay(1);
  }
  vTaskDelete(NULL);
}

static bool microphone_inference_start(uint32_t n_samples) {
  Serial.printf("Allocating buffer 0 (%u bytes)...\n", n_samples * sizeof(signed short));
  inference.buffers[0] = (signed short *)malloc(n_samples * sizeof(signed short));
  if (!inference.buffers[0]) {
    Serial.println("ERR: Buffer 0 malloc failed!");
    return false;
  }

  Serial.printf("Allocating buffer 1 (%u bytes)...\n", n_samples * sizeof(signed short));
  inference.buffers[1] = (signed short *)malloc(n_samples * sizeof(signed short));
  if (!inference.buffers[1]) {
    Serial.println("ERR: Buffer 1 malloc failed!");
    free(inference.buffers[0]);
    return false;
  }

  inference.buf_select = 0;
  inference.buf_count  = 0;
  inference.n_samples  = n_samples;
  inference.buf_ready  = 0;

  Serial.println("Initializing I2S for mic...");
  if (i2s_init(EI_CLASSIFIER_FREQUENCY) != 0) {
    Serial.println("ERR: I2S init failed!");
    return false;
  }

  record_status = true;
  Serial.println("Starting capture task...");
  // 24KB stack — ArduinoWebsockets sendBinary needs significant headroom
  xTaskCreatePinnedToCore(capture_samples, "CaptureSamples", 1024 * 24,
                          NULL, 10, NULL, 0);
  Serial.println("Mic inference started successfully!");
  return true;
}

static int microphone_audio_signal_get_data(size_t offset, size_t length, float *out_ptr) {
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
      .dma_buf_count = 4,
      .dma_buf_len = 512,
      .use_apll = false};

  i2s_pin_config_t pin_config = {
      .mck_io_num  = I2S_PIN_NO_CHANGE,
      .bck_io_num  = 32,
      .ws_io_num   = 25,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num  = 33};

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  return i2s_set_pin(I2S_NUM_0, &pin_config);
}