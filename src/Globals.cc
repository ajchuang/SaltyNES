/*
Copyright (c) 2012-2017 Matthew Brennan Jones <matthew.brennan.jones@gmail.com>
Copyright (c) 2006-2011 Jamie Sanders
A NES emulator in WebAssembly. Based on vNES.
Licensed under GPLV3 or later
Hosted at: https://github.com/workhorsy/SaltyNES
*/

#include "SaltyNES.h"

SDL_Window*   Globals::g_window     = nullptr;
SDL_Renderer* Globals::g_renderer   = nullptr;
SDL_Texture*  Globals::g_screen     = nullptr;
TTF_Font*     Globals::g_osd_font   = nullptr;
SDL_Color*    Globals::g_osd_color  = nullptr;

size_t Globals::window_width = 2 * RES_WIDTH;
size_t Globals::window_height = 2 * RES_HEIGHT;

double Globals::CPU_FREQ_NTSC   = 1789772.5;
double Globals::CPU_FREQ_PAL    = 1773447.4;
int Globals::preferredFrameRate = 60;

// Microseconds per frame:
const double Globals::MS_PER_FRAME = 1000000.0 / 60;
// What value to flush memory with on power-up:
uint16_t Globals::memoryFlushValue = 0xFF;

bool Globals::disableSprites  = false;
bool Globals::palEmulation    = false;
bool Globals::enableSound     = true;
bool Globals::printFps        = false;

std::map<string, uint32_t> Globals::keycodes; //Java key codes
std::map<string, string> Globals::controls; //vNES controls codes
std::map<int, SDL_Joystick*> Globals::joysticks;
bool Globals::is_windows = false;
