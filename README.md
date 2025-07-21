# SshSerialServer
Arduino-based SSH to serial interface using an ESP32

# Accessing the serial console of a Raspberry Pi via SSH
Connect the following pins:
- Raspberry Pi GPIO pin 14 (UART TX) with ESP32 pin IO18
- Raspberry Pi GPIO pin 15 (UART RX) with ESP32 pin IO19
- Raspberry Pi 3v3 Power with ESP32 3v3 power
- Raspberry Pi GND with ESP32 GND
