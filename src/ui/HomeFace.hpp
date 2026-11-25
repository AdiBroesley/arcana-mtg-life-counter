#pragma once
#include "lvgl.h"
#include "core/GameState.hpp"
#include "FaceDesigns.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace mtg {
// One opponent as a replacement home face shows it.
struct HomeOpponent {
 unsigned seat=2;          // 2..n, matching ARCANA's player tabs
 std::string name;         // "P2" locally, the player's name in a network game
 std::int64_t life=0;
 bool online=true;
 bool editable=false;      // local seats and open network seats can be edited here
};

// What a replacement home face may read and request. AppUI implements it.
class HomeFaceHost {
public:
 virtual ~HomeFaceHost()=default;
 virtual const GameState& game()const=0;
 virtual std::vector<HomeOpponent> opponents()const=0;
 virtual const FaceDesign& palette()const=0;   // the selected colour scheme
 // Same grouping/Undo behaviour as knob edits on ARCANA's own face.
 virtual bool adjustOwnLife(std::int64_t delta)=0;
 virtual bool adjustOpponentLife(unsigned seat,std::int64_t delta)=0;
 virtual void openMenu()=0;
};

// Replaces ARCANA's single-player main life face (the Home view for player 1).
// Setup, menus, counters, commander damage and multiplayer stay ARCANA's.
// With a home face installed, the gear button is not shown: the face must call
// HomeFaceHost::openMenu() itself (for example on a vertical swipe).
class IHomeFace {
public:
 virtual ~IHomeFace()=default;
 // The host has cleaned the screen and set its background.
 virtual void build(lv_obj_t* screen,HomeFaceHost& host)=0;
 // Game or network state changed.
 virtual void refresh(HomeFaceHost& host)=0;
 // Knob turned on the home face. Return true when handled (for example while an
 // opponent is in front); false lets the host change the player's own life.
 virtual bool rotate(HomeFaceHost&,std::int64_t){return false;}
 // Called on every UI update while the face is shown.
 virtual void tick(HomeFaceHost&,std::uint32_t){}
 // True while an animation runs (keeps the device loop responsive).
 virtual bool animating()const{return false;}
};
}
