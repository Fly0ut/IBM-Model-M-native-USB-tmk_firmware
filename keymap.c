/*
Copyright 2011 Jun Wako <wakojun@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* 
 * Keymap for Phantom controller
 */
#include "keymap_common.h"



/* 0: Qwerty
 * ,___________________________________________________________________,
 * |Esc|F1 | F2| F2| F3| F4| F5| F6| F7| F8| F9|F10|F11|F12|    Psc|Slk|Pus|
 * |----------------------------------------------------------------------------------------------|
 * |  `|  1|  2|  3|  4|  5|  6|  7|  8|  9|  0|  -|  =|  Backs|Ins|Hom|PgU|    |Num|  /|  *|    -|
 * |----------------------------------------------------------------------------------------------|
 * |Tab  |  Q|  W|  E|  R|  T|  Y|  U|  I|  O|  P|  [|  ]|    \|Del|End|PgD|    |  7|  8|  9|     |
 * |----------------------------------------------------------------------------------------|     |
 * |Contro|  A|  S|  D|  F|  G|  H|  J|  K|  L| ;|  '| Enter   |   | Up|   |    |  4|  5|  6|  +  |
 * |----------------------------------------------------------------------------------------------|
 * |Shift   |  Z|  X|  C|  V|  B|  N|  M|  ,|  .|  /|    Shift |Lef|Dow|Rig|    |  1|  2|  3|     |
 * `----------------------------------------------------------------------------------------- Entr|
 * |CtrR|Alt  |Space                                |Alt  |CtrL|            |   |      0|  .|     |
 * `----------------------------------------------------------------------------------------------'
 */

const uint8_t keymaps[][MATRIX_ROWS][MATRIX_COLS] = {[0] =KEYMAP( \
       ESC, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10,  F11,  F12,       PSCR, SLCK, PAUS,      \
       GRV,  1,   2,  3, 4,  5,  6,  7,  8,  9,   0, MINS, PLUS, BSPC,  INS, HOME, PGUP,      NLCK, PSLS, PAST, PMNS,\
       TAB,  Q,   W,  E, R,  T,  Y,  U,  I,  O,   P, LBRC, RBRC, BSLS,  DEL,  END, PGDN,        P7,   P8,   P9, PPLS,\
       CAPS, A,   S,  D, F,  G,  H,  J,  K,  L,SCLN,  QUOT, ENT,                                P4,   P5,   P6,\
       LSFT, Z,   X,  C, V,  B,  N,  M,COMM,DOT, SLSH, RSFT,                   UP,              P1,   P2,   P3, PENT,\
       LCTL, LALT,             SPC,                RALT, RCLT,         LEFT, DOWN, RIGHT,        P0,      PDOT),
};