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

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>
#include <util/delay.h>
#include "print.h"
#include "debug.h"
#include "util.h"
#include "matrix.h"


#ifndef DEBOUNCE
       DEBOUNCE 10
#endif

static uint8_t debouncing = DEBOUNCE;

// bit array of key state(1:on, 0:off)
static matrix_row_t matrix[MATRIX_ROWS];
static matrix_row_t matrix_debouncing[MATRIX_ROWS];

#ifdef MATRIX_HAS_GHOST
static bool matrix_has_ghost_in_row(matrix_row_t row);
#endif
static matrix_row_t read_rows(void);
static void init_rows(void);
static void unselect_cols(void);
static void select_col(matrix_col_t col);

#ifndef SLEEP_LED_ENABLE
/* LEDs are on output compare pins OC1B OC1C
   This activates fast PWM mode on them.
   Prescaler 256 and 8-bit counter results in
   16000000/256/256 = 244 Hz blink frequency.
   LED_A: Caps Lock
   LED_B: Scroll Lock  */
/* Output on PWM pins are turned off when the timer
   reaches the value in the output compare register,
   and are turned on when it reaches TOP (=256). */
static
void setup_leds(void)
{
    /*
    TCCR1A |=      // Timer control register 1A
        (1<<WGM10) | // Fast PWM 8-bit
        (1<<COM1B1)| // Clear OC1B on match, set at TOP
        (1<<COM1C1); // Clear OC1C on match, set at TOP
    TCCR1B |=      // Timer control register 1B
        (1<<WGM12) | // Fast PWM 8-bit
        (1<<CS12);   // Prescaler 256
    OCR1B = LED_BRIGHTNESS;    // Output compare register 1B
    OCR1C = LED_BRIGHTNESS;    // Output compare register 1C
    // LEDs: LED_A -> PORTB6, LED_B -> PORTB7
    DDRB  |= (1<<6) | (1<<7);
    PORTB  &= ~((1<<6) | (1<<7));
    */
}
#endif

inline
matrix_row_t matrix_rows(void)
{
    return MATRIX_ROWS;
}

inline
matrix_col_t matrix_cols(void)
{
    return MATRIX_COLS;
}

void matrix_init(void)
{
    // To use PORTF disable JTAG with writing JTD bit twice within four cycles.
    //MCUCR |= (1<<JTD);
    //MCUCR |= (1<<JTD);

    // initialize row and col
    unselect_cols();
    init_rows();
#ifndef SLEEP_LED_ENABLE
    setup_leds();
#endif

    // initialize matrix state: all keys off
    for (matrix_row_t i = 0; i < MATRIX_ROWS; i++)  {
        matrix[i] = 0;
        matrix_debouncing[i] = 0;
    }
}

matrix_row_t matrix_scan(void)
{
    for (matrix_col_t col = 0; col < MATRIX_COLS; col++) {  // 0-7
        select_col(col);
        _delay_us(30);       // without this wait it won't read stable value.
        matrix_row_t rows = read_rows();

        for (matrix_row_t row = 0; row < MATRIX_ROWS; row++) {  // 0-15
            bool prev_bit = matrix_debouncing[row] & ((matrix_row_t)1<<col);
            bool curr_bit = rows & (1<<row);
            if (prev_bit != curr_bit) {
                matrix_debouncing[row] ^= ((matrix_row_t)1<<col);
                if (debouncing) {
                    dprint("bounce!: "); dprintf("%02X", debouncing); dprintln();
                }
                debouncing = DEBOUNCE;
            }
        }
        unselect_cols();
    }

    if (debouncing) {
        if (--debouncing) {
            _delay_ms(1);
        } else {
            for (matrix_row_t i = 0; i < MATRIX_ROWS; i++) {
                matrix[i] = matrix_debouncing[i];
            }
        }
    }

    return 1;
}

bool matrix_is_modified(void)
{
    if (debouncing) return false;
    return true;
}

inline
bool matrix_is_on(matrix_row_t row, matrix_col_t col)
{
    return (matrix[row] & ((matrix_row_t)1<<col));
}

inline
matrix_row_t matrix_get_row(matrix_row_t row)
{
    return matrix[row];
}

/*
*void matrix_print(void)
*{
*   print("\nr/c 0123456789ABCDEF\n");
*   for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
*       xprintf("%02X: %032lb\n", row, bitrev32(matrix_get_row(row)));
*   }
*}
*/

void matrix_print(void)
{
    print("\nr/c 01234567\n");
    for (matrix_row_t row = 0; row < matrix_rows(); row++) {
        phex(row); print(": ");
        pbin_reverse(matrix_get_row(row));
#ifdef MATRIX_HAS_GHOST
        if (matrix_has_ghost_in_row(row)) {
            print(" <ghost");
        }
#endif
        print("\n");
    }
}

#ifdef MATRIX_HAS_GHOST
inline
static bool matrix_has_ghost_in_row(matrix_row_t row)
{
    // no ghost exists in case less than 2 keys on
    if (((matrix[row] - 1) & matrix[row]) == 0)
        return false;

    // ghost exists in case same state as other row
    for (matrix_row_t i=0; i < MATRIX_ROWS; i++) {
        if (i != row && (matrix[i] & matrix[row]))
            return true;
    }
    return false;
}
#endif

matrix_row_t matrix_key_count(void)
{
    matrix_row_t count = 0;
    for (matrix_row_t i = 0; i < MATRIX_ROWS; i++) {
        count += bitpop32(matrix[i]);
    }
    return count;
}

/* Row pin configuration
 * row: 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
 * pin: C7 C6 C5 C4 C3 C2 C1 C0 E1 E0 D7 D6 D5 D4 D3 D2
 */
static void init_rows(void) {
    // Input with pull-up(DDR:0, PORT:1)
    //        0b76543210
    DDRC  &= ~0b11111111; //PC: 7 6 5 4 3 2 1 0
    PORTC |=  0b11111111; //PC: 7 6 5 4 3 2 1 0
    //ToDo: Issues starts on row 8 and rows >=8 don't register.
    //left ctl <-> alt, caps lock <-> ctl, back space <-> backslash
    //F6, F7, F8, 8, 9, 0, -, + , i, o, p, {, }, k, l, ;, ', ,, ., /, all numpad, nav cluster, arrows, and above nav.
    DDRE  &= ~(1 << 1 | 1 << 0); //PE: 1 0
    PORTE |=  (1 << 1 | 1 << 0); //PE: 1 0
    //DDRD  &= ~0b11111000; //PD: 7 6 5 4 3
    //PORTD |=  0b11111000; //PD: 7 6 5 4 3
    DDRD  &= ~(1 << 7 | 1 << 6 | 1 << 5 | 1 << 4 | 1 << 3 | 1 << 2);
    PORTD |=  (1 << 7 | 1 << 6 | 1 << 5 | 1 << 4 | 1 << 3 | 1 << 2);

}
static matrix_row_t read_rows(void)
{
    return (PINC&(1<<7) ? 0 : (1<<0)) |
           (PINC&(1<<6) ? 0 : (1<<1)) |
           (PINC&(1<<5) ? 0 : (1<<2)) |
           (PINC&(1<<4) ? 0 : (1<<3)) |
           (PINC&(1<<3) ? 0 : (1<<4)) |
           (PINC&(1<<2) ? 0 : (1<<5)) |
          //Here is where the inputs stop.
           (PINC&(1<<1) ? 0 : (1<<6)) |
           (PINC&(1<<0) ? 0 : (1<<7)) |
           (PINE&(1<<1) ? 0 : (1<<8)) |
           (PINE&(1<<0) ? 0 : (1<<9)) |
           (PIND&(1<<7) ? 0 : (1<<10)) |
           (PIND&(1<<6) ? 0 : (1<<11)) |
           (PIND&(1<<5) ? 0 : (1<<12)) |
           (PIND&(1<<4) ? 0 : (1<<13)) |
           (PIND&(1<<3) ? 0 : (1<<14)) |
           (PIND&(1<<2) ? 0 : (1<<15));
}

/* Column pin configuration
 * col:  0  1  2  3  4  5  6  7
 * pin: B0 E7 E6 F0 F1 F2 F3 F4
 */
static void unselect_cols(void)
{
    //Hi-Z(DDR:0, PORT:0) to unselect
    //DDRB &= ~(1<<0);
    //PORTB &= ~(1<<0);
    //        0b76543210
    DDRB  &= ~0b00000001; //PB: 0
    PORTB &= ~0b00000001; //PB: 0
    //        0b76543210
    DDRF  &= ~0b00011111;  //PF: 4 3 2 1 0
    PORTF &= ~0b00011111;  //PF: 4 3 2 1 0
    //DDRE  &= ~(1<<7 | 1<<6);  //PE: 7 6
    //PORTE &= ~(1<<7 | 1<<6);  //PE: 7 6
    //        0b76543210
    DDRE  &= ~0b11000000; //PE: 7 6
    PORTE &= ~0b11000000; //PE: 7 6
}

static void select_col(matrix_col_t col)
{
    switch(col)
    {
        case 0:
            DDRB  |=  (1<<0);
            PORTB &= ~(1<<0);
            break;
        case 1:
            DDRE  |=  (1<<7);
            PORTE &= ~(1<<7);
            break;
        case 2:
            DDRE  |=  (1<<6);
            PORTE &= ~(1<<6);
            break;
        case 3:
            DDRF  |=  (1<<0);
            PORTF &= ~(1<<0);
            break;
        case 4:
            DDRF  |=  (1<<1);
            PORTF &= ~(1<<1);
            break;
        case 5:
            DDRF  |=  (1<<2);
            PORTF &= ~(1<<2);
            break;
        case 6:
            DDRF  |=  (1<<3);
            PORTF &= ~(1<<3);
            break;
        case 7:
            DDRF  |=  (1<<4);
            PORTF &= ~(1<<4);
            break;
    }
}