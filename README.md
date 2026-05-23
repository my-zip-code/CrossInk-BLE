## Experimental BLE Page Turner Build

This fork adds an experimental `ble_minimal` PlatformIO build for BLE HID page-turner support on CrossInk 1.3.0.

The work references the prior CrossPoint BLE implementation by `thedrunkpenguin/crosspoint-reader-ble`, adapted for CrossInk 1.3.0:

https://github.com/thedrunkpenguin/crosspoint-reader-ble

### What works

- Adds a `ble_minimal` firmware variant.
- Adds a Bluetooth Page Turner option to the in-book reader menu.
- Allows scanning for nearby BLE HID devices.
- Allows selecting and saving a BLE device.
- Allows forgetting the saved BLE device.
- Injects BLE page-turner input into CrossInk as virtual button presses.
- Tested with a Free2 BLE page turner.

### Known limitations

- This is experimental.
- BLE uses extra memory, so EPUB indexing/re-indexing can be slower.
- When CrossInk needs to index or re-index a chapter, BLE is temporarily disabled and the saved device is reconnected afterwards.
- Reconnection after indexing may take a few seconds.
- Changing reader settings such as font size can trigger re-indexing.
- Other BLE HID page turners may appear in the scan list, but may need additional device profiles or report mappings before their buttons work correctly.

### Recommended use

For the smoothest experience:

1. Open the book first.
2. Let the chapter/book index using the device buttons.
3. Open the in-book menu.
4. Select **Bluetooth Page Turner**.
5. Put your device in pairing mode.
6. Scan and connect.
7. Return to the book.

If page turning becomes unreliable after changing font size or reader layout settings, wait for indexing to finish, then reconnect the saved BLE device from the Bluetooth Page Turner menu.

## Features omitted in `ble_minimal`

To reduce memory pressure, the `ble_minimal` build omits some heavier optional features, including:

- KOReader sync
- Screenshot support
- Extra large / huge / emoji fonts

Depending on the current branch state, SD card font support may be enabled or disabled while testing memory use.
