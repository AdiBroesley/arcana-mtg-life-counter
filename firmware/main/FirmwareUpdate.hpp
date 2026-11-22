#pragma once
#include "platform/IFirmwareUpdate.hpp"
#include "core/FirmwareRelease.hpp"
#include "core/GameState.hpp"
#include "Wifi.hpp"
#include "esp_http_server.h"
#include <atomic>
#include <mutex>
namespace mtg {
class FirmwareUpdate final:public IFirmwareUpdate {
public:
 explicit FirmwareUpdate(WifiTransport& radio,GameState& game):radio_(radio),game_(game){}
 void begin();void poll();void diagnose()const;
 UpdateStatus status()const override;
 void check()override;void install()override;void setup()override;void cancel()override;
 bool active()const;
 static void confirmBoot();
private:
 void state(UpdatePhase phase,const std::string& message);
 void connect(bool install);void launch();void worker();void stopPortal();
 static void run(void* context);
 static esp_err_t portalPage(httpd_req_t* request);
 static esp_err_t portalSave(httpd_req_t* request);
 bool readCredentials(std::string& ssid,std::string& password);
 WifiTransport& radio_;GameState& game_;
 mutable std::mutex mutex_;
 UpdateStatus status_;FirmwareRelease release_;
 std::atomic<bool> running_{false},cancelled_{false},done_{false},saved_{false};
 bool installing_=false,pendingSetup_=false,ownsRadio_=false;
 std::uint64_t deadline_=0,restartAt_=0;
 std::string portalToken_;
 httpd_handle_t portal_=nullptr;
};
}
