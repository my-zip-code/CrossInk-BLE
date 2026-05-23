#pragma once

#include <string>
#include <vector>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct BluetoothPageTurnerDeviceRow {
  std::string address;
  std::string name;
  int rssi = 0;
  bool isHID = false;
};

enum class BluetoothPageTurnerState {
  READY,
  SCANNING,
  DEVICE_LIST,
  CONNECTING,
  CONNECTED,
  FAILED,
  NOT_AVAILABLE
};

class BluetoothPageTurnerActivity final : public Activity {
  ButtonNavigator buttonNavigator;

  BluetoothPageTurnerState state = BluetoothPageTurnerState::READY;
  std::vector<BluetoothPageTurnerDeviceRow> devices;
  int selectedIndex = 0;

  bool pendingScan = false;
  bool pendingConnect = false;
  bool pendingReconnect = false;

  std::string selectedAddress;
  std::string selectedName;
  std::string statusMessage;

  void beginScan();
  void performScan();
  void beginConnect();
  void performConnect();
  void performReconnect();
  void moveSelection(bool forward);
  std::string getSignalStrengthIndicator(int rssi) const;

  void renderReady() const;
  void renderScanning() const;
  void renderDeviceList() const;
  void renderConnecting() const;
  void renderConnected() const;
  void renderFailed() const;
  void renderNotAvailable() const;

 public:
  explicit BluetoothPageTurnerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("BluetoothPageTurner", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool isReaderActivity() const override { return true; }
  bool allowPowerAsConfirmInReaderMode() const override { return true; }
};
