Raw BLE advertisement tool
====
This tool is for receiving/sending raw BLE advertisement message.
The code is based on the Bluez library. It works on Linux only (maybe).

The makefile is configured for Raspberry Pi 3 at this point. LIB_DIRECTORY needs to be set properly for other environments.

## Preparation
Install dependencies.

```
sudo apt-get install libdbus-1-dev libdbus-glib-1-dev libglib2.0-dev libical-dev libreadline-dev libudev-dev libusb-dev make
sudo apt-get install bluetooth bluez-utils 
```

## Make
```
make
```

## How to use
The following shows the usage.
```
./bletool -h
```

If you want to receive advertisements, use -r option to set the tool *receive mode*.
```
./bletool -r
```

If you want to send advetisement, use -s option to set the tool *send mode*.
```
./bletool -s 00112233445566778899
```
The parameter following "-s" is the hex string of message.


