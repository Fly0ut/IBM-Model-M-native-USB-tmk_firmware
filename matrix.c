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
#include <util/delay.h>
#include "ch.h"
#include "hal.h"
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
    palSetPadMode(TEENSY_PIN0_IOPORT , TEENSY_PIN0,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN1_IOPORT , TEENSY_PIN1,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN2_IOPORT , TEENSY_PIN2,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN3_IOPORT , TEENSY_PIN3,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN4_IOPORT , TEENSY_PIN4,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN5_IOPORT , TEENSY_PIN5,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN6_IOPORT , TEENSY_PIN6,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN7_IOPORT , TEENSY_PIN7,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN8_IOPORT , TEENSY_PIN8,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN9_IOPORT , TEENSY_PIN9,  PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN10_IOPORT, TEENSY_PIN10, PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN11_IOPORT, TEENSY_PIN11, PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN12_IOPORT, TEENSY_PIN12, PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN14_IOPORT, TEENSY_PIN14, PAL_MODE_INPUT_PULLUP);
    palSetPadMode(TEENSY_PIN15_IOPORT, TEENSY_PIN15, PAL_MODE_INPUT_PULLUP);
}

static matrix_row_t read_rows(void)
{
    return (
            ((palReadPad(TEENSY_PIN0_IOPORT, TEENSY_PIN0)==PAL_HIGH) ? 0 : (1<<0)) |
            ((palReadPad(TEENSY_PIN1_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<1)) |
            ((palReadPad(TEENSY_PIN2_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<2)) |
            ((palReadPad(TEENSY_PIN3_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<3)) |
            ((palReadPad(TEENSY_PIN4_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<4)) |
            ((palReadPad(TEENSY_PIN5_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<5)) |
            ((palReadPad(TEENSY_PIN6_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<6)) |
            ((palReadPad(TEENSY_PIN7_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<7)) |
            ((palReadPad(TEENSY_PIN8_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<8)) |
            ((palReadPad(TEENSY_PIN9_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<9)) |
            ((palReadPad(TEENSY_PIN10_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<10)) |
            ((palReadPad(TEENSY_PIN11_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<11)) |
            ((palReadPad(TEENSY_PIN12_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<12)) |
            ((palReadPad(TEENSY_PIN13_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<13)) |
            ((palReadPad(TEENSY_PIN14_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<14)) |
            ((palReadPad(TEENSY_PIN15_IOPORT, TEENSY_PIN1)==PAL_HIGH) ? 0 : (1<<15))
           );
}

/* Column pin configuration
 * col:  0  1  2  3  4  5  6  7
 * pin: B0 E7 E6 F0 F1 F2 F3 F4
 */
static void unselect_cols(void)
{
    palSetPadMode(TEENSY_PIN16_IOPORT, TEENSY_PIN5, PAL_MODE_INPUT);
    palSetPadMode(TEENSY_PIN17_IOPORT, TEENSY_PIN5, PAL_MODE_INPUT);
    palSetPadMode(TEENSY_PIN18_IOPORT, TEENSY_PIN5, PAL_MODE_INPUT);
    palSetPadMode(TEENSY_PIN19_IOPORT, TEENSY_PIN5, PAL_MODE_INPUT);
    palSetPadMode(TEENSY_PIN20_IOPORT, TEENSY_PIN5, PAL_MODE_INPUT);
    palSetPadMode(TEENSY_PIN21_IOPORT, TEENSY_PIN5, PAL_MODE_INPUT);
    palSetPadMode(TEENSY_PIN22_IOPORT, TEENSY_PIN5, PAL_MODE_INPUT);
    palSetPadMode(TEENSY_PIN23_IOPORT, TEENSY_PIN5, PAL_MODE_INPUT);
}

static void select_col(matrix_col_t col)
{
    switch(col)
    {
        case 0:
            palSetPadMode(TEENSY_PIN16_IOPORT, TEENSY_PIN16, PAL_MODE_OUTPUT_PUSHPULL);
            palClearPad(TEENSY_PIN16_IOPORT, TEENSY_PIN16);
            break;
        case 1:
            palSetPadMode(TEENSY_PIN17_IOPORT, TEENSY_PIN17, PAL_MODE_OUTPUT_PUSHPULL);
            palClearPad(TEENSY_PIN17_IOPORT, TEENSY_PIN17);
            break;
        case 2:
            palSetPadMode(TEENSY_PIN18_IOPORT, TEENSY_PIN18, PAL_MODE_OUTPUT_PUSHPULL);
            palClearPad(TEENSY_PIN18_IOPORT, TEENSY_PIN18);
            break;
        case 3:
            palSetPadMode(TEENSY_PIN19_IOPORT, TEENSY_PIN19, PAL_MODE_OUTPUT_PUSHPULL);
            palClearPad(TEENSY_PIN19_IOPORT, TEENSY_PIN19);
            break;
        case 4:
            palSetPadMode(TEENSY_PIN20_IOPORT, TEENSY_PIN20, PAL_MODE_OUTPUT_PUSHPULL);
            palClearPad(TEENSY_PIN20_IOPORT, TEENSY_PIN20);
            break;
        case 5:
            palSetPadMode(TEENSY_PIN21_IOPORT, TEENSY_PIN21, PAL_MODE_OUTPUT_PUSHPULL);
            palClearPad(TEENSY_PIN21_IOPORT, TEENSY_PIN21);
            break;
        case 6:
            palSetPadMode(TEENSY_PIN22_IOPORT, TEENSY_PIN22, PAL_MODE_OUTPUT_PUSHPULL);
            palClearPad(TEENSY_PIN22_IOPORT, TEENSY_PIN22);
            break;
        case 7:
            palSetPadMode(TEENSY_PIN23_IOPORT, TEENSY_PIN23, PAL_MODE_OUTPUT_PUSHPULL);
            palClearPad(TEENSY_PIN23_IOPORT, TEENSY_PIN23);
            break;
    }
}