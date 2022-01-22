/*
Copyright (c) 2012-2017 Matthew Brennan Jones <matthew.brennan.jones@gmail.com>
Copyright (c) 2006-2011 Jamie Sanders
A NES emulator in WebAssembly. Based on vNES.
Licensed under GPLV3 or later
Hosted at: https://github.com/workhorsy/SaltyNES
*/

#include "SaltyNES.h"

static const uint32_t TOTAL_KEY_CNT = 255;

InputHandler::InputHandler(int id) :
  _is_input_pressed(NUM_KEYS, false),
  _id(id),
  _keys(TOTAL_KEY_CNT),
  _map(InputHandler::NUM_KEYS), enable_shared_from_this<InputHandler>() {

  _is_keyboard_used = false;
}

InputHandler::~InputHandler() {
}

uint16_t InputHandler::getKeyState(int padKey) {
  return static_cast<uint16_t>(_keys[_map[padKey]] ? 0x41 : 0x40);
}

void InputHandler::mapKey(int padKey, int kbKeycode) {
  _map[padKey] = kbKeycode;
}

void InputHandler::handler_non_nes_keys(const uint8_t* const keystate) {
  for (auto& it : m_user_keys) {
    const uint32_t key = it.first;
    const bool prv_state = it.second.first;
    const bool cur_state = keystate[key];

    if (prv_state == false and cur_state == true) {
      // press event
      it.second.second->on_key_down();
      it.second.first = cur_state;
    } else if (prv_state == true and cur_state == false) {
      // release event
      it.second.second->on_key_up();
      it.second.first = cur_state;
    }
  }
}

bool InputHandler::register_user_key(uint32_t code, UserKeyHandlerIntf* const handler) {
  if (code >= TOTAL_KEY_CNT) {
    return false;
  }

  // same key register twice
  if (m_user_keys.count(code)) {
    printf("duplicated key registration: %u\n", code);
    return false;
  }

  m_user_keys[code] = std::make_pair(false, handler);
  return true;
}

bool InputHandler::any_user_key_pressed(const uint8_t* const keystate) {
  for (auto& it : m_user_keys) {
    if (keystate[it.first])
      return true;
  }
  return false;
}

void InputHandler::poll_for_key_events() {
  // Check for keyboard input
  const uint8_t* const keystate = SDL_GetKeyboardState(nullptr);
  _keys[_map[InputHandler::KEY_UP]] =     keystate[SDL_SCANCODE_W];
  _keys[_map[InputHandler::KEY_DOWN]] =   keystate[SDL_SCANCODE_S];
  _keys[_map[InputHandler::KEY_RIGHT]] =  keystate[SDL_SCANCODE_D];
  _keys[_map[InputHandler::KEY_LEFT]] =   keystate[SDL_SCANCODE_A];
  _keys[_map[InputHandler::KEY_START]] =  keystate[SDL_SCANCODE_RETURN];
  _keys[_map[InputHandler::KEY_SELECT]] = keystate[SDL_SCANCODE_RSHIFT];
  _keys[_map[InputHandler::KEY_B]] =      keystate[SDL_SCANCODE_J];
  _keys[_map[InputHandler::KEY_A]] =      keystate[SDL_SCANCODE_K];

  const bool is_using_keyboard = (
      keystate[SDL_SCANCODE_W] |
      keystate[SDL_SCANCODE_S] |
      keystate[SDL_SCANCODE_D] |
      keystate[SDL_SCANCODE_A] |
      keystate[SDL_SCANCODE_RETURN] |
      keystate[SDL_SCANCODE_RSHIFT] |
      keystate[SDL_SCANCODE_J] |
      keystate[SDL_SCANCODE_K] |
      any_user_key_pressed(keystate)) != 0;

  // handler non-nes key events
  handler_non_nes_keys(keystate); 

  // Check for gamepad input
  if (!is_using_keyboard) {
    for (auto const& pair : Globals::joysticks) {
      int id = pair.first;
      SDL_Joystick* joy = pair.second;
      if (joy != nullptr && SDL_JoystickGetAttached(joy)) {
        if (Globals::is_windows) {
          _keys[_map[InputHandler::KEY_START]] = SDL_JoystickGetButton(joy, 9);
          _keys[_map[InputHandler::KEY_SELECT]] = SDL_JoystickGetButton(joy, 8);
          _keys[_map[InputHandler::KEY_B]] = SDL_JoystickGetButton(joy, 0);
          _keys[_map[InputHandler::KEY_A]] = SDL_JoystickGetButton(joy, 1);
          _keys[_map[InputHandler::KEY_UP]] = SDL_JoystickGetButton(joy, 12);
          _keys[_map[InputHandler::KEY_DOWN]] = SDL_JoystickGetButton(joy, 13);
          _keys[_map[InputHandler::KEY_RIGHT]] = SDL_JoystickGetButton(joy, 15);
          _keys[_map[InputHandler::KEY_LEFT]] = SDL_JoystickGetButton(joy, 14);
        } else {
          _keys[_map[InputHandler::KEY_START]] = SDL_JoystickGetButton(joy, 7);
          _keys[_map[InputHandler::KEY_SELECT]] = SDL_JoystickGetButton(joy, 6);
          _keys[_map[InputHandler::KEY_B]] = SDL_JoystickGetButton(joy, 0);
          _keys[_map[InputHandler::KEY_A]] = SDL_JoystickGetButton(joy, 1);
          _keys[_map[InputHandler::KEY_UP]] = SDL_JoystickGetButton(joy, 13);
          _keys[_map[InputHandler::KEY_DOWN]] = SDL_JoystickGetButton(joy, 14);
          _keys[_map[InputHandler::KEY_RIGHT]] = SDL_JoystickGetButton(joy, 12);
          _keys[_map[InputHandler::KEY_LEFT]] = SDL_JoystickGetButton(joy, 11);
        }
      }
    }
  }

  // Can't hold both left & right or up & down at same time:
  if(_keys[_map[InputHandler::KEY_LEFT]]) {
    _keys[_map[InputHandler::KEY_RIGHT]] = false;
  } else if(_keys[_map[InputHandler::KEY_RIGHT]]) {
    _keys[_map[InputHandler::KEY_LEFT]] = false;
  }

  if(_keys[_map[InputHandler::KEY_UP]]) {
    _keys[_map[InputHandler::KEY_DOWN]] = false;
  } else if(_keys[_map[InputHandler::KEY_DOWN]]) {
    _keys[_map[InputHandler::KEY_UP]] = false;
  }
}

void InputHandler::reset() {
  size_t size = _keys.size();
  _keys.clear();
  _keys.resize(size);
}

void InputHandler::key_down(uint32_t key) {
	_is_keyboard_used = true;
	switch(key) {
		case(38): _is_input_pressed[KEY_UP]     = true; break; // up = 38
		case(37): _is_input_pressed[KEY_LEFT]   = true; break; // left = 37
		case(40): _is_input_pressed[KEY_DOWN]   = true; break; // down = 40
		case(39): _is_input_pressed[KEY_RIGHT]  = true; break; // right = 39
		case(13): _is_input_pressed[KEY_START]  = true; break; // enter = 13
		case(17): _is_input_pressed[KEY_SELECT] = true; break; // ctrl = 17
		case(90): _is_input_pressed[KEY_B]      = true; break; // z = 90
		case(88): _is_input_pressed[KEY_A]      = true; break; // x = 88
	}
}

void InputHandler::key_up(uint32_t key) {
	_is_keyboard_used = true;
	switch(key) {
		case(38): _is_input_pressed[KEY_UP]     = false; break; // up = 38
		case(37): _is_input_pressed[KEY_LEFT]   = false; break; // left = 37
		case(40): _is_input_pressed[KEY_DOWN]   = false; break; // down = 40
		case(39): _is_input_pressed[KEY_RIGHT]  = false; break; // right = 39
		case(13): _is_input_pressed[KEY_START]  = false; break; // enter = 13
		case(17): _is_input_pressed[KEY_SELECT] = false; break; // ctrl = 17
		case(90): _is_input_pressed[KEY_B]      = false; break; // z = 90
		case(88): _is_input_pressed[KEY_A]      = false; break; // x = 88
	}
}

