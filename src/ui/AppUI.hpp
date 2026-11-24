#pragma once
#include "lvgl.h"
#include "core/GameState.hpp"
#include "core/RandomTools.hpp"
#include "core/Multiplayer.hpp"
#include "platform/IHardware.hpp"
#include "platform/IFirmwareUpdate.hpp"
#include "platform/IStorage.hpp"
#include "Widgets/ToolIcons.hpp"
#include <deque>
#include "FaceLayouts.hpp"
namespace mtg {
class AppUI {
public:
 AppUI(GameState& game,IHardware& hardware,lv_obj_t* screen,IRandom& random,DeviceProfile* profile=nullptr,Multiplayer* multiplayer=nullptr,IFirmwareUpdate* updates=nullptr);
 void update();
 bool lastUpdateDidWork()const{return lastUpdateWork_;}
 void goHome();
 void openTrackers() { if(game_.state().active) navigate(View::Menu); }
 void showStartup() { navigate(View::Startup); }
 void requestNewGame();
 void undo() { handle(Undo,0); }
 void redo() { handle(Redo,0); }
 void setFaceDesign(unsigned design);
 unsigned faceDesign()const{return faceDesign_;}
 FaceLayout faceLayoutGeometry()const{return redesignedFace(faceDesign_)?activeFaceLayout_:mtg::faceLayout(faceDesign_);}
 bool faceControlsFit()const;
 bool lifeOnlyIdle()const{return idleLifeView_;}
 bool animating()const{return transitionActive_||transitionPending_;}
 // Device firmware: autosave the active single-player match (2 s after the last
 // change) and offer Resume on the start screen after a reset. Undo history is
 // not saved. Multiplayer matches are not saved.
 void setMatchStorage(IStorage* storage);
 bool canResume()const{return !resumeData_.empty();}
 static constexpr std::uint32_t autosaveDelayMs=2000;
 // Starting multiplayer below this battery level asks for a second tap first.
 static constexpr unsigned lowBatteryRadioPercent=5;
 struct RenderTrial {unsigned phase;std::uint64_t micros;};
 // USB-only controlled comparison. Requires the single-player main life face.
 std::vector<RenderTrial> benchmarkUndoDrawing();
private:
 void decorateFaceConcept();void refreshFaceConcept();
 void buildFaceLayout();void refreshFaceLayout();bool redesignedLifeHoldValid()const;
 FaceLayout activeFaceLayout_;
 std::array<lv_obj_t*,2> layoutLifeTargets_{};
 std::vector<lv_obj_t*> layoutControls_;
 unsigned faceDesign_=0,facePage_=0;int conceptBatteryState_=-2;
 void saveBrightness();void saveDisplayDraft();void applyDisplaySettings(const DisplaySettings& value);
 void buildIdleDial();void refreshIdleDial();
 bool idleLifeView_=false;unsigned displayDraft_=5;
 unsigned observedBrightness_=100;std::uint32_t brightnessChangedAt_=0;bool brightnessPending_=false;
 enum class View { Display, DimDelay, IdleBrightness, Connection, FaceSelect, ContactCode, About, Updates, WifiSetup, Startup, ArcanaMode, NameSetup, HostJoin, BrowseGames, NetworkLobby, FormatSelect, PlayerSetup, EndConfirm, Gear, Brightness, ResetConfirm, CoinResult, RollResult, CounterPicker, Home, Menu, Counter, Sources, Damage, Commanders, Casts, Profiles, Status, History, Rename, AddSource, Threshold };
 enum Action { ResumeGame,OpenDisplay,OpenDimDelay,OpenIdleBrightness,ToggleLifeOnly,ToggleWakeProtection,OpenConnection,ChooseConnection,OpenFaces,ChooseFace,FacePage,ContactLink, OpenAbout, OpenUpdates, CheckUpdate, InstallUpdate, SetupWifi, SingleArcana,MultiArcana,NameKey,NameChange,NameDelete,NameSpace,NameSymbols,NameConfirm,HostGame,FindGames,JoinGame,RefreshGames,NextGames,StartNetwork, ToggleRecipient, ChooseFormat, NoAction, OpenBrightness, ToolBack, SideStep, OpenPicker, ChooseShortcut, RecentUndo, OpenGear, AskReset, ResetYes, ResetNo, FlipCoin, RollD20, NewGame, ConfirmPlayers, SelectPlayer, CycleSource, CloseDial, RequestEnd, FinishGame, CancelEnd, Back, Menu, Minus, Plus, OpenCounter, OpenSources, OpenOwn, OpenProfiles, OpenStatus,
  OpenHistory, Undo, Redo, Apply, Mode, NewSource, CreateSource, NewOwn, NewTurn, Rename,
  Append, Delete, SaveName, Limit, ToggleCounter, Source, Own, ToggleStatus, Day };
 struct Binding { AppUI* ui; Action action; std::uint32_t arg; double sectorStart=0,sectorSpan=0; };
 static void undoSectorHit(lv_event_t* event);
 static void toolSectorHit(lv_event_t* event);
 static void brightnessChanged(lv_event_t* event);
 static void clicked(lv_event_t* event);
 static void lifeTouch(lv_event_t* event);
 static void playerWedgeHit(lv_event_t* event);
 void repeatLifeHold(std::uint32_t now);
 bool lifeHoldPositionValid() const;
 void handle(Action action,std::uint32_t arg);
 void navigate(View view);
 void adjust(std::int64_t delta);
 void build();
 void advanceTransition();
 void setContentOpacity(lv_opa_t opacity);
 void buildDial();
 void buildTools();
 void buildMultiplayer();
 void refreshMultiplayer();
 void beginArcanaSelection(View returnTo);
 bool networkFace()const{return multiplayer_&&multiplayer_->attached()&&multiplayer_->connected()&&multiplayer_->started();}
 lv_obj_t* radialSector(const std::string& title,unsigned index,unsigned count,Action action,std::uint32_t arg,unsigned color,widgets::ToolIcon icon);
 void radialBack(Action action=ToolBack);
 void buildPicker();
 lv_obj_t* roundPopup();
 void counterShortcuts();
 void refreshDial();
 void refreshBattery();
 void curvedUndoText(const std::string& value);
 void setRecentUndoAvailable(bool available);
 void buildLifecycle();
 void buildInformation();
 void refreshInformation();
 void beginFormatSelection(View returnTo);
 void refreshLifecycle();
 void playerTabs();
 void refreshTabs();
 void selectPlayer(unsigned player);
 void dialFrame();
 void refresh();
 void header(const std::string& title,const std::string& subtitle="");
 void historyButtons();
 lv_obj_t* list();
 void row(lv_obj_t* parent,int index,const std::string& title,const std::string& detail,Action action,std::uint32_t arg=0,unsigned color=0xb798ff);
 lv_obj_t* button(lv_obj_t* parent,const std::string& value,int x,int y,int w,int h,Action action,std::uint32_t arg=0,unsigned color=0x1d1c30);
 lv_obj_t* cutButton(lv_obj_t* circle,const std::string& value,int x,int y,int w,int h,Action action,std::uint32_t arg=0);
 void counterRockers();
 void partialDividers();
 lv_obj_t* bigValue(int y=115);
 void stepButtons(int y=216);
 IFirmwareUpdate* updates_;UpdateStatus updateStatus_;std::uint64_t updateRevision_=UINT64_MAX;
 lv_obj_t* updateButton_=nullptr;lv_obj_t* newGameButton_=nullptr;
 lv_obj_t* setupQr_=nullptr;std::string setupQrPayload_;
 DeviceProfile fallbackProfile_;DeviceProfile* profile_;Multiplayer* multiplayer_;
 std::string nameDraft_,networkSignature_;bool nameSymbols_=false,hostPending_=false;unsigned lobbyPage_=0;View arcanaReturn_=View::Startup;View informationReturn_=View::Startup;
 GameState& game_;
 IHardware& hardware_;
 IRandom& random_;
 unsigned contactLink_=0;
 unsigned pickerSide_=0;
 bool coinHeads_=false;
 StarterRoll starterRoll_;
 lv_obj_t* screen_;
 GameFormat draftFormat_=GameFormat::Commander;
 View formatReturn_=View::Startup;
 View view_=View::Startup, renameReturn_=View::Home, endReturn_=View::Gear;
 bool lifeHoldActive_=false, lifeHoldCancelled_=false;
 int lifeHoldSign_=1;
 std::uint32_t lifeHoldStarted_=0,lifeHoldRepeats_=0;
 Counter counter_=Counter::Poison;
 std::uint32_t sourceId_=1;
 std::size_t ownIndex_=0;
 std::int64_t pending_=1;
 unsigned owner_=2, character_=0, selectedPlayer_=1, draftPlayers_=4;
 std::array<lv_obj_t*,12> tabValues_{};
 std::string undoCurvedText_;
 bool freezeUndoCaption_=false;
 bool subtractLife_=true, rebuild_=true, refreshDirty_=true, lastUpdateWork_=false;
 bool transitionPending_=false,transitionActive_=false;
 IStorage* matchStorage_=nullptr;std::string resumeData_;
 std::uint64_t savedRevision_=0;std::uint32_t saveDueAt_=0;bool saveDue_=false,savedActive_=false;
 bool lowBatteryWarned_=false;
 void autosaveMatch();
 bool lowBatteryForRadio()const;
 std::uint32_t transitionStarted_=0,transitionFrame_=0;
 lv_obj_t* fadeCover_=nullptr;
 std::string draft_, notice_;
 std::uint64_t revision_=0;
 std::uint64_t networkRevision_=0;
 std::deque<Binding> bindings_;
 std::deque<std::array<lv_point_precise_t,2>> tabEdgePoints_;
 int batteryState_=-2;
 lv_obj_t* battery_=nullptr;
 lv_obj_t* undoArc_=nullptr;
 lv_obj_t* brightnessSlider_=nullptr;
 lv_obj_t *recipientLabel_=nullptr,*lifeMarker_=nullptr,*lifeRight_=nullptr,*value_=nullptr,*detail_=nullptr,*hint_=nullptr,*aux_=nullptr,*status_=nullptr,*poison_=nullptr,*damage_=nullptr,*scroll_=nullptr,*apply_=nullptr,*mode_=nullptr,*undo_=nullptr,*redo_=nullptr;
};
}
