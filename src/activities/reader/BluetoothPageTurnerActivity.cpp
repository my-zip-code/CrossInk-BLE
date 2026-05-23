#include "BluetoothPageTurnerActivity.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>

#if !defined(SIMULATOR) && defined(ENABLE_BLE_PAGE_TURNER)
#include <BluetoothHIDManager.h>
#endif

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr uint32_t BLE_SCAN_DURATION_MS = 3000;

bool bluetoothNameLooksLikePageTurner(const std::string& name) {
  return name.find("Free") != std::string::npos || name.find("free") != std::string::npos ||
         name.find("Page") != std::string::npos || name.find("page") != std::string::npos ||
         name.find("HID") != std::string::npos || name.find("hid") != std::string::npos;
}

const char* stateTitle(BluetoothPageTurnerState state) {
  switch (state) {
    case BluetoothPageTurnerState::READY:
      return "Bluetooth Page Turner";
    case BluetoothPageTurnerState::SCANNING:
      return "Scanning";
    case BluetoothPageTurnerState::DEVICE_LIST:
      return "Select Device";
    case BluetoothPageTurnerState::CONNECTING:
      return "Connecting";
    case BluetoothPageTurnerState::CONNECTED:
      return "Connected";
    case BluetoothPageTurnerState::FAILED:
      return "Connection Failed";
    case BluetoothPageTurnerState::NOT_AVAILABLE:
      return "Bluetooth Unavailable";
  }
  return "Bluetooth Page Turner";
}
}  // namespace

#if !defined(SIMULATOR) && defined(ENABLE_BLE_PAGE_TURNER)
namespace {
void configureBluetoothPageTurnerCallbacks() {
  auto& btMgr = BluetoothHIDManager::getInstance();

  btMgr.setButtonInjector([](uint8_t buttonIndex, bool pressed) {
    gpio.setVirtualButtonState(buttonIndex, pressed);
  });

  btMgr.setButtonActivityNotifier([](uint8_t buttonIndex) {
    gpio.updateVirtualButtonActivity(buttonIndex);
  });

  // This activity is only reachable from the in-book reader menu.
  btMgr.setReaderContextCallback([]() {
    return true;
  });
}
}  // namespace
#endif

void BluetoothPageTurnerActivity::onEnter() {
  Activity::onEnter();

#if !defined(SIMULATOR) && defined(ENABLE_BLE_PAGE_TURNER)
  configureBluetoothPageTurnerCallbacks();

  auto& btMgr = BluetoothHIDManager::getInstance();
  if (!btMgr.enable()) {
    state = BluetoothPageTurnerState::FAILED;
    statusMessage = std::string("Bluetooth enable failed: ") + btMgr.lastError;
    requestUpdate();
    return;
  }

  if (btMgr.hasBondedDevice()) {
    state = BluetoothPageTurnerState::CONNECTING;
    pendingReconnect = true;

    std::string name = btMgr.getBondedDeviceName();
    if (name.empty()) {
      name = "saved device";
    }
    statusMessage = std::string("Reconnecting to ") + name + "...";
  } else {
    state = BluetoothPageTurnerState::READY;
    statusMessage = "Put your device in pairing mode, then press OK to scan.";
  }
#else
  state = BluetoothPageTurnerState::NOT_AVAILABLE;
  statusMessage = "This firmware was not built with BLE support.";
#endif

  requestUpdate();
}

void BluetoothPageTurnerActivity::onExit() {
  Activity::onExit();

#if !defined(SIMULATOR) && defined(ENABLE_BLE_PAGE_TURNER)
  auto& btMgr = BluetoothHIDManager::getInstance();

  // If the user leaves without a connected device, release BLE RAM again.
  // If connected, keep BLE alive so the page turner works in the reader.
  if (btMgr.isEnabled() && btMgr.getConnectedDevices().empty()) {
    btMgr.disable();
  }
#endif
}

void BluetoothPageTurnerActivity::beginScan() {
#if !defined(SIMULATOR) && defined(ENABLE_BLE_PAGE_TURNER)
  configureBluetoothPageTurnerCallbacks();

  state = BluetoothPageTurnerState::SCANNING;
  pendingScan = true;
  statusMessage = "Scanning for nearby BLE devices...";
  requestUpdate();
#else
  state = BluetoothPageTurnerState::NOT_AVAILABLE;
  requestUpdate();
#endif
}

void BluetoothPageTurnerActivity::performScan() {
#if !defined(SIMULATOR) && defined(ENABLE_BLE_PAGE_TURNER)
  pendingScan = false;

  auto& btMgr = BluetoothHIDManager::getInstance();
  if (!btMgr.enable()) {
    state = BluetoothPageTurnerState::FAILED;
    statusMessage = std::string("Bluetooth enable failed: ") + btMgr.lastError;
    requestUpdate();
    return;
  }

  btMgr.startScan(BLE_SCAN_DURATION_MS);

  devices.clear();
  const auto& found = btMgr.getDiscoveredDevices();
  devices.reserve(found.size());

  for (const auto& device : found) {
    BluetoothPageTurnerDeviceRow row;
    row.address = device.address;
    row.name = device.name.empty() ? "Unknown" : device.name;
    row.rssi = device.rssi;
    row.isHID = device.isHID;
    devices.push_back(row);
  }

  std::sort(devices.begin(), devices.end(), [](const auto& a, const auto& b) {
    const bool aLooksUseful = a.isHID || bluetoothNameLooksLikePageTurner(a.name);
    const bool bLooksUseful = b.isHID || bluetoothNameLooksLikePageTurner(b.name);
    if (aLooksUseful != bLooksUseful) {
      return aLooksUseful > bLooksUseful;
    }
    return a.rssi > b.rssi;
  });

  selectedIndex = 0;
  state = BluetoothPageTurnerState::DEVICE_LIST;
  if (devices.empty()) {
    statusMessage = "No BLE devices found. Put your device in pairing mode and press Right to retry.";
  } else {
    statusMessage = "Choose your device. HID-like devices are listed first.";
  }
  requestUpdate();
#endif
}

void BluetoothPageTurnerActivity::beginConnect() {
#if !defined(SIMULATOR) && defined(ENABLE_BLE_PAGE_TURNER)
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(devices.size())) {
    beginScan();
    return;
  }

  selectedAddress = devices[selectedIndex].address;
  selectedName = devices[selectedIndex].name;
  state = BluetoothPageTurnerState::CONNECTING;
  pendingConnect = true;
  statusMessage = std::string("Connecting to ") + selectedName + "...";
  requestUpdate();
#endif
}

void BluetoothPageTurnerActivity::performConnect() {
#if !defined(SIMULATOR) && defined(ENABLE_BLE_PAGE_TURNER)
  pendingConnect = false;

  configureBluetoothPageTurnerCallbacks();

  auto& btMgr = BluetoothHIDManager::getInstance();
  if (!btMgr.enable()) {
    state = BluetoothPageTurnerState::FAILED;
    statusMessage = std::string("Bluetooth enable failed: ") + btMgr.lastError;
    requestUpdate();
    return;
  }

  if (btMgr.connectToDevice(selectedAddress)) {
    btMgr.setBondedDevice(selectedAddress, selectedName);
    btMgr.saveState();
    state = BluetoothPageTurnerState::CONNECTED;
    statusMessage = selectedName.empty() ? "Connected." : std::string("Connected to ") + selectedName + ".";
  } else {
    state = BluetoothPageTurnerState::FAILED;
    statusMessage = std::string("Could not connect to ") + selectedName + ": " + btMgr.lastError;
  }

  requestUpdate();
#endif
}

void BluetoothPageTurnerActivity::performReconnect() {
#if !defined(SIMULATOR) && defined(ENABLE_BLE_PAGE_TURNER)
  pendingReconnect = false;

  configureBluetoothPageTurnerCallbacks();

  auto& btMgr = BluetoothHIDManager::getInstance();

  if (btMgr.connectToBondedDevice()) {
    state = BluetoothPageTurnerState::CONNECTED;

    std::string name = btMgr.getBondedDeviceName();
    if (name.empty()) {
      name = "saved device";
    }
    statusMessage = std::string("Reconnected to ") + name + ".";
  } else {
    state = BluetoothPageTurnerState::READY;
    statusMessage = "Saved device not available. Press OK to scan.";
  }

  requestUpdate();
#endif
}

void BluetoothPageTurnerActivity::moveSelection(bool forward) {
  if (devices.empty()) return;
  selectedIndex = forward ? ButtonNavigator::nextIndex(selectedIndex, static_cast<int>(devices.size()))
                          : ButtonNavigator::previousIndex(selectedIndex, static_cast<int>(devices.size()));
}

std::string BluetoothPageTurnerActivity::getSignalStrengthIndicator(const int rssi) const {
  if (rssi >= -50) return "||||";
  if (rssi >= -60) return " |||";
  if (rssi >= -70) return "  ||";
  return "   |";
}

void BluetoothPageTurnerActivity::loop() {
  if (state == BluetoothPageTurnerState::CONNECTING && pendingReconnect) {
    performReconnect();
    return;
  }

  if (state == BluetoothPageTurnerState::SCANNING && pendingScan) {
    performScan();
    return;
  }

  if (state == BluetoothPageTurnerState::CONNECTING && pendingConnect) {
    performConnect();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (state == BluetoothPageTurnerState::READY) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      beginScan();
      return;
    }
  }

  if (state == BluetoothPageTurnerState::DEVICE_LIST) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (devices.empty()) {
        beginScan();
      } else {
        beginConnect();
      }
      return;
    }

    if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      beginScan();
      return;
    }

    buttonNavigator.onNext([this] {
      moveSelection(true);
      requestUpdate();
    });

    buttonNavigator.onPrevious([this] {
      moveSelection(false);
      requestUpdate();
    });
  }

  if (state == BluetoothPageTurnerState::FAILED) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
        mappedInput.wasPressed(MappedInputManager::Button::Right)) {
      beginScan();
      return;
    }
  }

  if (state == BluetoothPageTurnerState::CONNECTED) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      finish();
      return;
    }
  }
}

void BluetoothPageTurnerActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  char countStr[32] = {0};
  if (state == BluetoothPageTurnerState::DEVICE_LIST) {
    snprintf(countStr, sizeof(countStr), "%u found", static_cast<unsigned>(devices.size()));
  }

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, stateTitle(state),
                 countStr[0] ? countStr : nullptr);

  switch (state) {
    case BluetoothPageTurnerState::READY:
      renderReady();
      break;
    case BluetoothPageTurnerState::SCANNING:
      renderScanning();
      break;
    case BluetoothPageTurnerState::DEVICE_LIST:
      renderDeviceList();
      break;
    case BluetoothPageTurnerState::CONNECTING:
      renderConnecting();
      break;
    case BluetoothPageTurnerState::CONNECTED:
      renderConnected();
      break;
    case BluetoothPageTurnerState::FAILED:
      renderFailed();
      break;
    case BluetoothPageTurnerState::NOT_AVAILABLE:
      renderNotAvailable();
      break;
  }

  renderer.displayBuffer();
}

void BluetoothPageTurnerActivity::renderReady() const {
  const auto pageHeight = renderer.getScreenHeight();
  const int top = pageHeight / 2 - 20;

  renderer.drawCenteredText(UI_10_FONT_ID, top, statusMessage.c_str());
  renderer.drawCenteredText(SMALL_FONT_ID, top + 30, "Put your device in pairing mode first.");

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Scan", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BluetoothPageTurnerActivity::renderScanning() const {
  const auto pageHeight = renderer.getScreenHeight();
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BluetoothPageTurnerActivity::renderDeviceList() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  if (devices.empty()) {
    const int top = pageHeight / 2 - 20;
    renderer.drawCenteredText(UI_10_FONT_ID, top, "No BLE devices found.");
    renderer.drawCenteredText(SMALL_FONT_ID, top + 30, "Press Right to scan again.");
  } else {
    const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
    const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;

    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(devices.size()), selectedIndex,
        [this](int index) {
          const auto& device = devices[index];
          std::string name = device.name.empty() ? "Unknown" : device.name;
          if (name.length() > 22) {
            name = name.substr(0, 19) + "...";
          }
          return name;
        },
        nullptr, nullptr,
        [this](int index) {
          const auto& device = devices[index];
          std::string flags;
          if (device.isHID) flags += "HID ";
          if (bluetoothNameLooksLikePageTurner(device.name)) flags += "PT ";
          flags += getSignalStrengthIndicator(device.rssi);
          return flags;
        },
        true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), devices.empty() ? "Scan" : "Connect", "", "Retry");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BluetoothPageTurnerActivity::renderConnecting() const {
  const auto pageHeight = renderer.getScreenHeight();
  const int top = pageHeight / 2 - 20;

  renderer.drawCenteredText(UI_12_FONT_ID, top - 30, "Connecting", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, top + 10, statusMessage.c_str());

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BluetoothPageTurnerActivity::renderConnected() const {
  const auto pageHeight = renderer.getScreenHeight();
  const int top = pageHeight / 2 - 45;

  renderer.drawCenteredText(UI_12_FONT_ID, top, "Connected", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, top + 35, statusMessage.c_str());
  renderer.drawCenteredText(SMALL_FONT_ID, top + 65, "Press Back to return to the book.");

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BluetoothPageTurnerActivity::renderFailed() const {
  const auto pageHeight = renderer.getScreenHeight();
  const int top = pageHeight / 2 - 30;

  renderer.drawCenteredText(UI_12_FONT_ID, top, "Connection Failed", true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(UI_10_FONT_ID, top + 40, statusMessage.c_str());
  renderer.drawCenteredText(SMALL_FONT_ID, top + 70, "Press OK or Right to scan again.");

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Scan", "", "Retry");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BluetoothPageTurnerActivity::renderNotAvailable() const {
  const auto pageHeight = renderer.getScreenHeight();
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, statusMessage.c_str());

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
