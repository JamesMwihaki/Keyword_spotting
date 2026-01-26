# ESP32 Keyword Spotting

## Edge computing at its finest.
- In this project, I am using an ESP32 connected to a microphone to listen for phrases.
- In this case, the phrases I trained the TinyML on were "drop green screen" and "raise green screen." I also included some noise and background sounds in the training dataset.
- I used Edge Impulse for data collection and ML, and deployed the model as an Arduino library.
- I have my ESP32 connected to two motors such that "drop green screen" moves the motor at ID[0] forward and "raise green screen" moves the motor at ID[1] backward.

## Features
- **Real-Time Edge Inference**: Performs local keyword spotting directly on the ESP32 using a quantized TinyML model, eliminating the need for cloud processing or internet latency.
- **High-Fidelity Audio Capture**: Utilizes the I2S protocol with an INMP441 MEMS microphone for 16-bit, low-noise digital audio sampling.
- **Voice-Activated Actuation**: Integrated motor control logic that maps specific vocal intents ("Drop" vs. "Raise") to physical hardware movement.
- **Optimized Resource Management**: Efficient memory handling to run both the Wi-Fi stack and the Machine Learning inference engine within the ESP32’s SRAM limits.

## 🏗 Architecture & Hardware

The system captures raw audio data, processes it through a Signal Processing (DSP) block, and runs inference using a Neural Network-all on the edge.

### Hardware Requirements
* **Microcontroller**: ESP32
* **Microphone**: INMP441 I2S Omnidirectional Microphone
* **Actuator**: DC Motor controlled via L298N (or similar) motor driver
* **Power**: External 5V/12V source for the motor
Or a board that has most of this included.

### Wiring Diagram
| INMP441 Pin | ESP32 Pin | Function |
| :--- | :--- | :--- |
| VDD | 3V3 | Power |
| GND | GND | Ground |
| SCK | GPIO 14 | I2S Serial Clock |
| WS | GPIO 25 | I2S Word Select |
| SD | GPIO 32 | I2S Serial Data |
| L/R | GND | Left Channel |

## 🧠 Machine Learning Model
The model was built using **Edge Impulse**.

* **Dataset**: AI-generated voices (OpenAI).
* **Classes**: `Drop`, `Raise`, `Background`.
* **DSP Block**: Spectrogram / MFE (Mel Frequency Energy).
* **Inference Engine**: EON™ Compiler.

### ESP32 Firmware
- Flash the code in the `Main` directory to your ESP32 using the Arduino IDE.
-Access Point: The ESP32 operates as an Access Point. You can customize the SSID and Password in NetworkConfig.cpp.

## Challenges with this approach
- False positives: When they occurred, an action was performed in the real world (WE CAN'T HAVE THAT).
- The system was biased against me: The AI-generated training voices did not account for my thick Kenyan accent. This resulted in poor recognition for the primary user (me!).
- 
## Project improvements
- Eliminate false positives causing action in the physical world.
- The safety check does not do its work as much as I would like.
- Train the TinyML to include my voice (imagine creating something that refuses to listen to you—sorry to all the moms!!!).

## ⚖️ License
This project is open-source under the [MIT License](LICENSE).
