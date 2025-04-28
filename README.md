# ESP Verwaltung - ESP32 Updater System

A simple system for updating ESP32 S3 devices with the latest firmware.

## Updater Applications

This repository contains two updater applications:

- **ESP-UPDATER-Server.exe** - Updates ESP32 S3 server devices
- **ESP-UPDATER-Client.exe** - Updates ESP32 S3 client devices

## How to Use the Updaters

### Updating an ESP32 Server Device

1. Connect your ESP32 S3 server device to your computer via USB
2. Run `ESP-UPDATER-Server.exe`
3. Click the "Start" button
4. The application will automatically:
   - Download the latest Arduino CLI
   - Clone the latest ESP1Server firmware
   - Install the ESP32 board support package
   - Compile the firmware
   - Detect your ESP32 device
   - Upload the firmware using 115200 baud rate
5. Monitor the progress in the log window
6. When complete, the log will show "Update & Upload erfolgreich!"

### Updating an ESP32 Client Device

1. Connect your ESP32 S3 client device to your computer via USB
2. Run `ESP-UPDATER-Client.exe`
3. Click the "Start" button
4. The application will automatically:
   - Download the latest Arduino CLI
   - Clone the latest ESP2Client firmware
   - Install the ESP32 board support package
   - Compile the firmware
   - Detect your ESP32 device
   - Upload the firmware using 115200 baud rate
5. Monitor the progress in the log window
6. When complete, the log will show "Update & Upload erfolgreich!"

## Troubleshooting

- **No device detected:** Make sure your ESP32 S3 device is properly connected via USB
- **Upload failed:** Try pressing the reset button on your ESP32 device and run the updater again
- **Compilation errors:** These are shown in the log window - check if there are any specific error messages

## Building the Updaters (For Developers)

If you need to build the updater executables:

1. Clone this repository
2. Run the build script:
   ```
   python build_esp_updater.py
   ```
3. The executables will be created in the `dist` folder
