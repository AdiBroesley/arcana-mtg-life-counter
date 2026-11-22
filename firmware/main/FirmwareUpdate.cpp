#include "FirmwareUpdate.hpp"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_pm.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_sntp.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/sha256.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <functional>
#include <strings.h>
namespace mtg {
namespace {
std::uint64_t now(){return esp_timer_get_time()/1000;}
bool trustedUrl(const std::string& url){
 for(const char* prefix:{"https://api.github.com/","https://github.com/","https://release-assets.githubusercontent.com/","https://objects.githubusercontent.com/"})
  if(url.compare(0,strlen(prefix),prefix)==0)return true;
 return false;
}
std::string decode(const std::string& text){
 std::string result;
 for(std::size_t i=0;i<text.size();++i){
  if(text[i]=='+')result+=' ';
  else if(text[i]=='%'&&i+2<text.size()){
   auto hex=[](char c)->int{if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;};
   int a=hex(text[i+1]),b=hex(text[i+2]);if(a<0||b<0)return {};result+=static_cast<char>(a*16+b);i+=2;
  }else result+=text[i];
 }
 return result;
}
constexpr const char* portalHtml=R"HTML(<!doctype html><html lang="en"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ARCANA Wi-Fi</title><style>body{background:#07131e;color:#f5fbff;font:16px system-ui;max-width:420px;margin:10vh auto;padding:24px}h1{letter-spacing:.25em;color:#00d8ff}p{color:#9db7c5}label{display:block;margin-top:24px}input,button{box-sizing:border-box;width:100%;padding:14px;border-radius:12px;margin-top:8px;border:1px solid #126580;background:#0b2130;color:white;font:inherit}button{margin-top:28px;background:#007e68;cursor:pointer}small{color:#9db7c5}</style><h1>ARCANA</h1><h2>Connect for updates</h2><p>Choose a 2.4 GHz Wi-Fi network or phone hotspot. Your settings stay on this device.</p><form method="post" action="/save"><input type="hidden" name="token" value="@TOKEN@"><label>Wi-Fi network name<input name="ssid" maxlength="32" required autocomplete="off"></label><label>Password<input name="password" type="password" maxlength="63" autocomplete="new-password"></label><button>Save and connect</button></form><p><small>Leave the password empty for an open network. Return to ARCANA to check the connection.</small></p></html>)HTML";
struct HttpContext {
 std::atomic<bool>* cancelled=nullptr;
 std::string body;
 esp_ota_handle_t ota=0;
 std::uint32_t bytes=0,expected=0;
 mbedtls_sha256_context hash{};
 std::array<std::uint8_t,sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t)+sizeof(esp_app_desc_t)> header{};
 std::size_t headerBytes=0;
 bool headerChecked=false;
 std::string version,error,redirect;unsigned redirects=0;
 std::function<void(unsigned)> progress;
};
esp_err_t httpEvent(esp_http_client_event_t* event){
 auto& c=*static_cast<HttpContext*>(event->user_data);
 if(c.cancelled->load()||!c.error.empty())return ESP_FAIL;
 if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&!strcasecmp(event->header_key,"Location")){
  c.redirect=event->header_value;
  if(!trustedUrl(c.redirect)){c.error="Unexpected download redirect";return ESP_FAIL;}
 }
 if(event->event_id==HTTP_EVENT_REDIRECT){
  if(c.redirect.empty()||++c.redirects>4){c.error="Unexpected download redirect";return ESP_FAIL;}
  c.redirect.clear();return esp_http_client_set_redirection(event->client);
 }
 if(event->event_id!=HTTP_EVENT_ON_DATA||esp_http_client_get_status_code(event->client)!=200)return ESP_OK;
 if(!c.ota){if(c.body.size()+event->data_len>131072){c.error="Release response too large";return ESP_FAIL;}c.body.append(static_cast<const char*>(event->data),event->data_len);return ESP_OK;}
 if(static_cast<std::uint64_t>(c.bytes)+event->data_len>c.expected){c.error="Unexpected firmware size";return ESP_FAIL;}
 if(!c.headerChecked){
  auto copy=std::min(c.header.size()-c.headerBytes,static_cast<std::size_t>(event->data_len));memcpy(c.header.data()+c.headerBytes,event->data,copy);c.headerBytes+=copy;
  if(c.headerBytes==c.header.size()){
   esp_app_desc_t description{};memcpy(&description,c.header.data()+sizeof(esp_image_header_t)+sizeof(esp_image_segment_header_t),sizeof(description));
   if(description.magic_word!=ESP_APP_DESC_MAGIC_WORD||std::string(description.project_name,strnlen(description.project_name,sizeof(description.project_name)))!="arcana"||std::string(description.version,strnlen(description.version,sizeof(description.version)))!=c.version){c.error="Wrong ARCANA firmware version";return ESP_FAIL;}
   c.headerChecked=true;
  }
 }
 if(esp_ota_write(c.ota,event->data,event->data_len)!=ESP_OK||mbedtls_sha256_update(&c.hash,static_cast<const unsigned char*>(event->data),event->data_len)!=0){c.error="Firmware write failed";return ESP_FAIL;}
 c.bytes+=event->data_len;c.progress(static_cast<unsigned>(100ULL*c.bytes/c.expected));return ESP_OK;
}
int request(const std::string& url,HttpContext& context){
 esp_http_client_config_t config{};config.url=url.c_str();config.crt_bundle_attach=esp_crt_bundle_attach;
 const std::string agent=std::string("ARCANA/")+appInfo::version;
 config.buffer_size=4096;config.buffer_size_tx=4096;
 config.timeout_ms=6000;config.max_redirection_count=4;config.disable_auto_redirect=true;config.user_agent=agent.c_str();
 config.event_handler=httpEvent;config.user_data=&context;
 auto client=esp_http_client_init(&config);if(!client)throw std::runtime_error("Cannot open secure connection");
 esp_http_client_set_header(client,"Accept","application/vnd.github+json");
 auto result=esp_http_client_perform(client);int status=esp_http_client_get_status_code(client);esp_http_client_cleanup(client);
 if(result!=ESP_OK||!context.error.empty())throw std::runtime_error(context.error.empty()?"Download interrupted. Try again.":context.error);
 return status;
}
}
UpdateStatus FirmwareUpdate::status()const{std::lock_guard<std::mutex> lock(mutex_);return status_;}
void FirmwareUpdate::state(UpdatePhase phase,const std::string& message){std::lock_guard<std::mutex> lock(mutex_);status_.phase=phase;status_.message=message;++status_.revision;}
bool FirmwareUpdate::active()const{auto s=status();return running_||s.busy()||s.phase==UpdatePhase::Setup;}
bool FirmwareUpdate::readCredentials(std::string& ssid,std::string& password){
 nvs_handle_t handle;if(nvs_open("arcana_update",NVS_READONLY,&handle)!=ESP_OK)return false;
 std::size_t length=0;auto result=nvs_get_blob(handle,"wifi",nullptr,&length);std::string value(length,'\0');
 if(result==ESP_OK&&length>0&&length<256)result=nvs_get_blob(handle,"wifi",value.data(),&length);else result=ESP_FAIL;
 nvs_close(handle);if(result!=ESP_OK)return false;
 try{auto j=nlohmann::json::parse(value);ssid=j.at("ssid").get<std::string>();password=j.at("password").get<std::string>();return !ssid.empty()&&ssid.size()<=32&&password.size()<=63;}catch(...){return false;}
}
void FirmwareUpdate::begin(){check();}
void FirmwareUpdate::connect(bool install){
 if(game_.state().active||running_||status().phase==UpdatePhase::Installing)return;
 stopPortal();if(radio_.updateNetwork())radio_.stopNetwork();
 std::string ssid,password;if(!readCredentials(ssid,password)){state(UpdatePhase::Offline,"Set up Wi-Fi to check releases");return;}
 cancelled_=false;installing_=install;ownsRadio_=true;
 if(install){std::lock_guard<std::mutex> lock(mutex_);status_.progress=0;}
 if(!radio_.joinInternet(ssid,password)){state(UpdatePhase::Error,"Could not start Wi-Fi");return;}
 deadline_=now()+20000;state(UpdatePhase::Connecting,"Connecting to Wi-Fi...");
}
void FirmwareUpdate::check(){if(!active())connect(false);}
void FirmwareUpdate::install(){if(status().phase==UpdatePhase::Available)connect(true);}
void FirmwareUpdate::stopPortal(){if(portal_){httpd_stop(portal_);portal_=nullptr;}}
void FirmwareUpdate::cancel(){
 if(status().phase==UpdatePhase::Installing||status().phase==UpdatePhase::Ready)return;
 pendingSetup_=false;cancelled_=true;stopPortal();if(radio_.updateNetwork())radio_.stopNetwork();ownsRadio_=false;
 if(status().phase!=UpdatePhase::Available&&status().phase!=UpdatePhase::Current)state(UpdatePhase::Offline,"Check updates in About");
}
void FirmwareUpdate::setup(){
 if(game_.state().active||status().phase==UpdatePhase::Installing)return;
 if(running_){cancelled_=true;pendingSetup_=true;state(UpdatePhase::Connecting,"Preparing Wi-Fi setup...");return;}
 cancel();cancelled_=false;
 char suffix[9];snprintf(suffix,sizeof(suffix),"%08X",static_cast<unsigned>(esp_random()));std::string network=std::string("ARCANA-SETUP-")+suffix;
 char key[13];snprintf(key,sizeof(key),"%08X%04X",static_cast<unsigned>(esp_random()),static_cast<unsigned>(esp_random()&65535));
 if(!radio_.setupNetwork(network,key)){state(UpdatePhase::Error,"Wi-Fi setup unavailable");return;}
 portalToken_=std::string(key)+suffix;
 ownsRadio_=true;{std::lock_guard<std::mutex> lock(mutex_);status_.setupNetwork=network;status_.setupPassword=key;}
 httpd_config_t config=HTTPD_DEFAULT_CONFIG();config.max_uri_handlers=2;config.stack_size=6144;config.lru_purge_enable=true;
 if(httpd_start(&portal_,&config)!=ESP_OK){radio_.stopNetwork();state(UpdatePhase::Error,"Wi-Fi setup unavailable");return;}
 httpd_uri_t page{};page.uri="/";page.method=HTTP_GET;page.handler=portalPage;page.user_ctx=this;
 httpd_uri_t save{};save.uri="/save";save.method=HTTP_POST;save.handler=portalSave;save.user_ctx=this;
 httpd_register_uri_handler(portal_,&page);httpd_register_uri_handler(portal_,&save);
 deadline_=now()+180000;state(UpdatePhase::Setup,"Connect using your phone");
}
esp_err_t FirmwareUpdate::portalPage(httpd_req_t* r){httpd_resp_set_type(r,"text/html");httpd_resp_set_hdr(r,"Cache-Control","no-store");std::string page=portalHtml;auto marker=page.find("@TOKEN@");page.replace(marker,7,static_cast<FirmwareUpdate*>(r->user_ctx)->portalToken_);return httpd_resp_send(r,page.data(),page.size());}
esp_err_t FirmwareUpdate::portalSave(httpd_req_t* r){
 if(r->content_len<=0||r->content_len>512)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid settings");
 std::string body(r->content_len,'\0');std::size_t read=0;
 while(read<body.size()){int count=httpd_req_recv(r,body.data()+read,body.size()-read);if(count<=0)return ESP_FAIL;read+=count;}
 std::string ssid,password,token;for(std::size_t offset=0;offset<body.size();){auto end=body.find('&',offset);if(end==std::string::npos)end=body.size();auto equal=body.find('=',offset);if(equal<end){auto key=body.substr(offset,equal-offset),value=decode(body.substr(equal+1,end-equal-1));if(key=="ssid")ssid=value;if(key=="password")password=value;if(key=="token")token=value;}offset=end+1;}
 if(token!=static_cast<FirmwareUpdate*>(r->user_ctx)->portalToken_)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Reopen the Wi-Fi setup page");
 if(ssid.empty()||ssid.size()>32||ssid.find('\0')!=std::string::npos||password.find('\0')!=std::string::npos||(!password.empty()&&(password.size()<8||password.size()>63)))return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Check network name and password length");
 std::string value;try{value=nlohmann::json{{"ssid",ssid},{"password",password}}.dump();}catch(...){return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Use a valid network name");}
 if(value.size()>=256)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Settings are too long");
 nvs_handle_t handle;
 if(nvs_open("arcana_update",NVS_READWRITE,&handle)!=ESP_OK)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Could not save Wi-Fi");
 auto result=nvs_set_blob(handle,"wifi",value.data(),value.size());if(result==ESP_OK)result=nvs_commit(handle);nvs_close(handle);
 if(result!=ESP_OK)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Could not save Wi-Fi");
 auto resultPage=httpd_resp_sendstr(r,"Wi-Fi saved. ARCANA is connecting. Return your phone to its normal Wi-Fi network.");
 if(resultPage==ESP_OK)static_cast<FirmwareUpdate*>(r->user_ctx)->saved_=true;return resultPage;
}
void FirmwareUpdate::launch(){running_=true;done_=false;state(installing_?UpdatePhase::Installing:UpdatePhase::Checking,installing_?"Installing firmware...":"Checking GitHub release...");
 if(xTaskCreatePinnedToCore(run,"arcana_update",16384,this,2,nullptr,1)!=pdPASS){running_=false;state(UpdatePhase::Error,"Not enough memory for update");}
}
void FirmwareUpdate::run(void* context){auto& self=*static_cast<FirmwareUpdate*>(context);self.worker();self.done_=true;self.running_=false;vTaskDelete(nullptr);}
void FirmwareUpdate::worker(){
 esp_pm_lock_handle_t cpu=nullptr,awake=nullptr;esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX,0,"update_cpu",&cpu);esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP,0,"update_flash",&awake);
 if(cpu)esp_pm_lock_acquire(cpu);if(awake)esp_pm_lock_acquire(awake);
 esp_ota_handle_t ota=0;
 try{
  if(std::time(nullptr)<1700000000){if(!esp_sntp_enabled()){esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_init();}
   auto end=now()+12000;while(!cancelled_&&std::time(nullptr)<1700000000&&now()<end)vTaskDelay(pdMS_TO_TICKS(100));
   if(std::time(nullptr)<1700000000)throw std::runtime_error("Could not set network time");}
  if(cancelled_)throw std::runtime_error("Check cancelled");
  const auto* partition=esp_ota_get_next_update_partition(nullptr);if(!partition)throw std::runtime_error("USB setup required for updates");
  if(!installing_){HttpContext context;context.cancelled=&cancelled_;
   int code=request(std::string("https://api.github.com/repos/")+appInfo::repository+"/releases/latest",context);
   if(code==404){state(UpdatePhase::Current,"No firmware releases yet");}
   else if(code!=200)throw std::runtime_error("GitHub unavailable. Try later.");
   else {auto release=FirmwareRelease::parse(context.body,partition->size);{std::lock_guard<std::mutex> lock(mutex_);release_=release;status_.latest=release.version;}
    if(!cancelled_)state(release.newerThan(appInfo::version)?UpdatePhase::Available:UpdatePhase::Current,release.newerThan(appInfo::version)?"New version ready to install":"You have the latest firmware");}
  }else{
   FirmwareRelease release;{std::lock_guard<std::mutex> lock(mutex_);release=release_;}
   if(!release.newerThan(appInfo::version))throw std::runtime_error("No newer firmware available");
   if(esp_ota_begin(partition,OTA_WITH_SEQUENTIAL_WRITES,&ota)!=ESP_OK)throw std::runtime_error("Cannot prepare firmware slot");
   HttpContext context;context.cancelled=&cancelled_;context.ota=ota;context.expected=release.bytes;context.version=release.version;
   context.progress=[this](unsigned progress){std::lock_guard<std::mutex> lock(mutex_);if(status_.progress!=progress){status_.progress=progress;++status_.revision;}};
   mbedtls_sha256_init(&context.hash);mbedtls_sha256_starts(&context.hash,0);
   int code=0;try{code=request(release.url,context);}catch(...){mbedtls_sha256_free(&context.hash);throw;}
   unsigned char hash[32];mbedtls_sha256_finish(&context.hash,hash);mbedtls_sha256_free(&context.hash);
   char digest[65];for(unsigned i=0;i<32;++i)snprintf(digest+2*i,3,"%02x",hash[i]);
   if(code!=200||context.bytes!=release.bytes||!context.headerChecked||release.sha256!=digest)throw std::runtime_error("Firmware verification failed");
   auto verified=esp_ota_end(ota);ota=0;
   if(verified!=ESP_OK||esp_ota_set_boot_partition(partition)!=ESP_OK)throw std::runtime_error("Firmware verification failed");
   state(UpdatePhase::Ready,"Update verified. Restarting...");
  }
 }catch(const std::runtime_error& error){if(!cancelled_)state(UpdatePhase::Error,error.what());}
 catch(...){if(!cancelled_)state(UpdatePhase::Error,"Could not read this release");}
 if(ota)esp_ota_abort(ota);
 if(esp_sntp_enabled())esp_sntp_stop();
 if(awake){esp_pm_lock_release(awake);esp_pm_lock_delete(awake);}if(cpu){esp_pm_lock_release(cpu);esp_pm_lock_delete(cpu);}
}
void FirmwareUpdate::poll(){
 if(game_.state().active&&active()&&!cancelled_)cancel();
 if(done_.exchange(false)){
  if(radio_.updateNetwork())radio_.stopNetwork();ownsRadio_=false;
  if(cancelled_)state(UpdatePhase::Offline,"Check updates in About");
  if(status().phase==UpdatePhase::Ready)restartAt_=now()+1200;
 }
 if(pendingSetup_&&!running_){pendingSetup_=false;setup();}
 if(saved_.exchange(false)){stopPortal();if(radio_.updateNetwork())radio_.stopNetwork();state(UpdatePhase::Offline,"Wi-Fi saved");connect(false);}
 auto phase=status().phase;
 if(phase==UpdatePhase::Connecting&&!running_&&!pendingSetup_){radio_.pollNetwork();if(radio_.internetReady())launch();else if(now()>deadline_){if(radio_.updateNetwork())radio_.stopNetwork();ownsRadio_=false;state(UpdatePhase::Error,"Check Wi-Fi name and password");}}
 if(phase==UpdatePhase::Setup&&now()>deadline_){cancel();state(UpdatePhase::Offline,"Wi-Fi setup timed out");}
 if(restartAt_&&now()>=restartAt_)esp_restart();
}
void FirmwareUpdate::diagnose()const{
 auto current=status();auto* partition=esp_ota_get_running_partition();auto* next=esp_ota_get_next_update_partition(nullptr);
 esp_ota_img_states_t imageState=ESP_OTA_IMG_UNDEFINED;auto imageResult=partition?esp_ota_get_state_partition(partition,&imageState):ESP_ERR_NOT_FOUND;
 printf("UPDATE version=%s phase=%u progress=%u latest=%s running=%s address=0x%lx next=%s image_state=%d free_internal=%lu message=%s\n",appInfo::version,static_cast<unsigned>(current.phase),current.progress,current.latest.c_str(),partition?partition->label:"none",static_cast<unsigned long>(partition?partition->address:0),next?next->label:"none",imageResult==ESP_OK?static_cast<int>(imageState):-1,static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),current.message.c_str());
 printf("UPDATE END\n");fflush(stdout);
}
void FirmwareUpdate::confirmBoot(){
 auto* partition=esp_ota_get_running_partition();esp_ota_img_states_t state;
 if(partition&&esp_ota_get_state_partition(partition,&state)==ESP_OK&&state==ESP_OTA_IMG_PENDING_VERIFY)ESP_ERROR_CHECK(esp_ota_mark_app_valid_cancel_rollback());
}
}
