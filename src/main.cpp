#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>

#include "defines.h"

#include "res/select.h"
#include "res/move.h"
#include "res/cancel.h"

#include "amp.h"
#include "keypad.h"
#include "tftui.h"
#include "config.h"
#include "playlist.h"
#include "browser.h"

#include "sound.h"
#include "eq.h"
#include "ay.h"
#include "ayflyw.h"

#include "players/ASCPlay.h"
#include "players/PSCPlay.h"
#include "players/PT1Play.h"
#include "players/PT2Play.h"
#include "players/PT3Play.h"
#include "players/SQTPlay.h"
#include "players/STCPlay.h"
#include "players/STPPlay.h"
#include "players/AYPlay.h"
#include "players/PSGPlay.h"

#include "player.h"

void setup(){
  display_brightness(0);
  config_load();
  AYPlayCoreInit();
  initVoltage();
  TFTInit();
  buttonsSetup();
  DACInit();
  AYInit();
  muteAYBeep();
  ampInit();
  introTFT();
  delay(2000);
  show_frame();
  checkSDonStart();
}

void loop(){
  ay_set_clock(Config.ay_clock);
  generalTick();
  player();
  switch(PlayerCTRL.screen_mode){
    case SCR_PLAYER:
      player_screen();
      break;
    case SCR_BROWSER:
      browser_screen(Config.isBrowserPlaylist);
      break;
    case SCR_CONFIG:
      config_screen();
      break;
    case SCR_RESET_CONFIG:
      config_reset_default_screen();
      break;
    case SCR_ABOUT:
      config_about_screen();
      break;
    case SCR_SDEJECT:
      sdEject();
      checkSDonStart();
      break;
  }
  scrTimeout();
}