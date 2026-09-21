# Tiny Lamp

Thi is my little tiny lamp project, it consist of two different parts, the lamp itself and a server to run the lamp

## the lamp software

The lamp software, for now will be written into a raspberry pi pico 2w and it will contain the interface to talk to the addressable LEDs. Later this will be ported to a ESP32-C3

The software will contain the engine to run the lamp as well as a way to connect to a wifi hotspot and a pairing mode to add the wifi hotspot to the device.

## the server

There will be an online server that will talk directly to the lamp. The server will be written using the Tanstack Start and upload it into vercel for ease of use. from there we can control the device by going into a website and controlling the devise.
