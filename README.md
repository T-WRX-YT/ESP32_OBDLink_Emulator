# ESP32 OBDLink Emulator
### Example code for emulating OBD2 and OBDLink CANBUS data for SoloStorm Telemetry

### This version uses the ESP-IDF framework.  It manually sets the Bluetooth link policy to prevent sniff mode, which allows canbus frames to not be interrupted by lack of receiving data



## What this does
This code will allow you to connect an ESP32 via Bluetooth to a tablet running the SoloStorm app, and allow you to send data to the app as either an OBD2 PID device, or act as an OBDLink device and send raw canbus packets for vehicle telemetry.

This code will automatically send CANBUS manually created data, or respond to OBD2 requests automatically depending on which option you chose in the app.



## Why would you use this
You would use this code if you wanted to:

- Send dummy/pre-planned data to SoloStorm to verify the channel mapping and CANBUS frame parsing works
- Send CANBUS/OBD2 data to SoloStorm as vehicle telemetry if you already are using an Arduino/ESP32/Teensy as a device hooked into your CAN system, and cannot add a second OBD2 port device due to data collisions



## OBD2 Handshake process
This is how the connection handshake works for OBD2 mode:

Luckily for us, since the app expects the specific device to respond we do not have to mimic the EXACT responses.  Therefore, for all of the received messages, you simply need to respond with 'OK' in ELM327 mode, which means 'OK' then a \r for carriage return, then a '>' to tell the device you are ready for more data.

- Receive ATZ (initialize the device)
- Receive ATE0 (turn echo responses off)
- Receive ATS0 (turn off spaces in the return data)
- Receive ATL0 (turn off newline chars in the response, only send /r)
- Receive ATSP00 (set the protocol to AUTO, which means this will be OBD2 from the app)
- Receive ATDP (requesting the current protocol, just going to send 'OK' since the app knows what's happening)

You will then receive standard OBD2 PID requests, one at a time, to which the code will respond with dummy data.  If you were to run this in production, you would be returning your real values back.

This method can achieve between 5 and 10 updates per second from what I've seen, CANBUS is much faster and better if you can use it.



## OBDLink CANBUS method
This will handshake with the app and allow for a stream of CANBUS messages to be sent, which then allows you to manually map data channels inside the app.  Since you can send any type of CANBUS message you want, you can tailor the data to match the exact inputs you want to record.

Luckily for us, since the app expects the specific device to respond we do not have to mimic the EXACT responses.  Therefore, for all of the received messages, you simply need to respond with 'OK' in ELM327 mode, which means 'OK' then a \r for carriage return, then a '>' to tell the device you are ready for more data.

- Receive ATZ (initialize the device)
- Receive ATE0 (turn echo responses off)
- Receive ATS0 (turn off spaces in the return data)
- Receive ATL0 (turn off newline chars in the response, only send /r)
- Receive STP 33 (set the protocol to ISO CANBUS)
- Receive STCSEGR (turns on inbound segmentation, which means you should be using ISO-TP if you need to send ONE big message)
- Receive ATH 1 (turn on printing CANBUS header.  IE: the CAN ID)
- Receive ATD 0 (turn OFF the DLC message, so you should JUST be sending 0x601 0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08)
- Receive ATPPS (this requests all of the settings in the OBDLink, but since the app doesn't need to know the answer you just send an 'OK')
- Receive STFAC (clear all filters)
- Receive STFPA 000,000 (sets the filter.  in logger mode it is all 0's, otherwise it will be based on your channel mappings.  **since you are sending JUST the can data you want, this should automatically match your data once you have your channels mapped, but in the end it doesn't matter because you are sending just your own data**)
- Receive STCMM 0 (this turns off ACK messages for receiving CANBUS frames)
- Receive STPO (open current protocol)
- Receive ATDP (requesting the current protocol, just going to send 'OK' since the app knows what's happening)
- STM (start the CANBUS monitor)

Once an STM command has been received, that means the app is expecting data, so the code will then start looping through CANBUS send messages.  You can insert your own data into here and it will show up in the app as vehicle telemetry which you can then map to data channels.

If an 'ATPC' message is received, the CAN messages will be stopped and wait for a renegotiation.
