This is a fork of https://github.com/walkco/stadia-vigem

# Stadia-ViGEm

Xbox 360 controller emulation for Stadia controller. Supports multiple devices and vibration. Forked from Mi-ViGEm (https://github.com/grayver/Mi-ViGEm) by grayver.
Xbox 360 emulation driver is provided by ViGEm (https://github.com/ViGEm/ViGEmBus), by Benjamin Höglinger.

## Requirements
- Windows 10 (should work on Windows 7 and 8 also)
- ViGEm bus installed (can be downloaded [here](https://github.com/ViGEm/ViGEmBus/releases))

## How it works
Stadia-ViGEm program at start scans for Stadia Controllers and then proxies found Stadia Controllers to virtual Xbox 360 gamepads (with help from ViGEmBus). Also Stadia-ViGEm subscribes to system device plug/unplug notifications and rescans for devices on each notification.
All found devices are displayed in the tray icon context menu. Manual device rescan can be initiated via the tray icon context menu.

## Double input
Stadia-ViGEm creates a virtual Xbox 360 controller which results in double input issues when some applications will read input from both the virtual and the real Stadia controller. To avoid this, install [HidHide](https://github.com/ViGEm/HidHide) and configure it as follows:
 - Open HidHide Configuration Client
 - On Applications tab:
   - Click "+" button
   - Browse to the Stadia-ViGEm executable you normally use (Stadia-ViGEm-x86.exe or Stadia-ViGEm-x64.exe)
 - On Devices tab:
   - Tick box next to the Stadia controller entry (my controller is named as "Google LLC Stadia Controller rev. A")
   - Tick "Enable device hiding" at the bottom of the window
 - Reboot your PC

After this, only Stadia-ViGEm will be able to see the real controller. Note: This means that whenever Stadia-ViGEm isn't running, the controller will not be able to control anything on your PC.

## Thanks to

grayver, the developer of Mi-ViGEm that makes up 95% of this program.

This project is inspired by following projects written on C#:
- https://github.com/irungentoo/Xiaomi_gamepad
- https://github.com/dancol90/mi-360

Thanks to following libraries and resources:
- https://github.com/libusb/hidapi for HID implementation
- https://github.com/zserge/tray for lightweight tray app implementation
- https://www.flaticon.com/authors/freepik for application icon
