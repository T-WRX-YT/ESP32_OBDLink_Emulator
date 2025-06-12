#include "BluetoothSerial.h"



// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;


String inputBuffer = "";
bool initComplete = false;
int protocol = 0;



void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32-SOLOSTORM-TEST");
  SerialBT.setPin("1234", 4);  // DONT USE THIS IN PRODUCTION
  //SerialBT.deleteAllBondedDevices(); // Uncomment this to delete paired devices; Must be called after begin
}


void loop() {
  while (SerialBT.available()) {
    char c = SerialBT.read();

    if (c == '\r' || c == '\n') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) {
        handleCmd(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }


  /* CAN FRAMES CAN SEND HERE, SEND VARIABLES OR ANYTHING */
  if (initComplete && protocol == 1) {
    uint8_t data[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};    
    sendCanFrameWithoutDLC(0x123, data, 8);
  }
  /* CAN FRAMES CAN SEND HERE, SEND VARIABLES OR ANYTHING */

  delay(20);
}

// make sure the can frame has no spaces, otherwise the app will ignore it and not parse
void sendCanFrameWithoutDLC(uint16_t canId, const uint8_t* data, uint8_t len) {
  char buffer[64];

  sprintf(buffer, "%03X", canId);
  SerialBT.print(buffer);
  //Serial.print("sent ");
  for (int i = 0; i < len; i++) {
    sprintf(buffer, "%02X", data[i]);
    SerialBT.print(buffer);
    //Serial.print(buffer);
  }
  //Serial.println();
  SerialBT.print("\r");
}


void sendResponse(String response) {
  Serial.println("Sending " + response);
  SerialBT.print(response);
  SerialBT.print("\r>");
}

void handleCmd(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  Serial.print("Received: " + cmd + " ");

  /* HANDLES OBD2 PID RESPONSES, INEFFECIENT BUT WORKS */
  if (cmd == "010C" || cmd == "0111" || cmd == "010B" || cmd == "010A" || cmd == "0105" || cmd == "015C" || cmd == "010F") {
    Serial.println("Received OBD command");

    if (cmd == "010C") {
      // Simulate 3000 RPM
      uint16_t rpm_raw = 3000 * 4; // 12000
      uint8_t A = (rpm_raw >> 8) & 0xFF;
      uint8_t B = rpm_raw & 0xFF;

      char buffer[32];
      sprintf(buffer, "41 0C %02X %02X", A, B);
      sendResponse(String(buffer));
    }
    else if (cmd == "0111") {
      // Simulate 50% throttle
      uint8_t A = 0x7F;  // ~50% throttle
      char buffer[16];
      sprintf(buffer, "41 11 %02X", A);
      sendResponse(String(buffer));
    }
    else if (cmd == "010B") {
      uint8_t A = 40;  // 40 kPa
      char buffer[16];
      sprintf(buffer, "41 0B %02X", A);
      sendResponse(String(buffer));
    }
    else if (cmd == "010A") {
      uint8_t A = 100;  // 100 × 3 = 300 kPa
      char buffer[16];
      sprintf(buffer, "41 0A %02X", A);
      sendResponse(String(buffer));
    }
    else if (cmd == "0105") {
      uint8_t A = 90 + 40;  // 130 = 0x82
      char buffer[16];
      sprintf(buffer, "41 05 %02X", A);
      sendResponse(String(buffer));
    }
    else if (cmd == "015C") {
      uint8_t A = 100 + 40;  // 140 = 0x8C
      char buffer[16];
      sprintf(buffer, "41 5C %02X", A);
      sendResponse(String(buffer));
    }
    else if (cmd == "010F") {
      uint8_t A = 35 + 40;  // 75 = 0x4B
      char buffer[16];
      sprintf(buffer, "41 0F %02X", A);
      sendResponse(String(buffer));
    }

  }
  /* THIS HANDLES THE HANDSHAKE AND AT COMMANDS, YOU JUST NEED TO RESPONSD WITH "OK\r>" */
  else {
    if (cmd == "ATZ") {
      Serial.println("[Init begin]");
      initComplete = false;
      protocol = 0;
      sendResponse("OK");
    }
    else if (cmd == "STP 33") {
      Serial.print("[Protocol: CANBUS] ");
      protocol = 1;
      sendResponse("OK");
    }
    else if (cmd == "ATSP00") {
      Serial.print("[Protocol: OBD2] ");
      protocol = 2;
      sendResponse("OK");
    }
    else if (cmd == "STM") {
      Serial.print("[Init complete] ");
      initComplete = 1;
      sendResponse("OK");
    }
    else if (cmd == "ATPC") {
      Serial.print("[Connection close] ");
      initComplete = false;
      protocol = 0;
      sendResponse("OK");
    }
    else if (cmd == "ATIGN") {
      Serial.print("[Connection close] ");
      initComplete = false;
      protocol = 0;
      sendResponse("WAKE");
    }
    else {
      sendResponse("OK");
    }
  }
}