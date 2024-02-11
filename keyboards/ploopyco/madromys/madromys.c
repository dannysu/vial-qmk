/* Copyright 2023 Colin Lam (Ploopy Corporation)
 * Copyright 2020 Christopher Courtney, aka Drashna Jael're  (@drashna) <drashna@live.com>
 * Copyright 2019 Sunjun Kim
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "madromys.h"

// DPI Settings
#ifndef PLOOPY_DPI_OPTIONS
#    define PLOOPY_DPI_OPTIONS \
        { 400, 600, 800, 1200 }
#    ifndef PLOOPY_DPI_DEFAULT
#        define PLOOPY_DPI_DEFAULT 1
#    endif
#endif
#ifndef PLOOPY_DPI_DEFAULT
#    define PLOOPY_DPI_DEFAULT 0
#endif

// Drag Scroll Settings
#ifndef PLOOPY_DRAGSCROLL_DPI
#    define PLOOPY_DRAGSCROLL_DPI 800
#endif
#ifndef PLOOPY_DRAGSCROLL_INVERT
#    define PLOOPY_DRAGSCROLL_INVERT 1
#endif
#ifndef PLOOPY_DRAGSCROLL_MOMENTARY
#    define PLOOPY_DRAGSCROLL_MOMENTARY 0
#endif
#ifndef PLOOPY_DRAGSCROLL_ANY_MOUSE_KEYCODE_TOGGLES_OFF
#    define PLOOPY_DRAGSCROLL_ANY_MOUSE_KEYCODE_TOGGLES_OFF 1
#endif
#ifndef PLOOPY_SCROLL_DIVISORS
#    define PLOOPY_SCROLL_DIVISORS \
        { 256.0, 128.0, 64.0 }
#    ifndef PLOOPY_SCROLL_DIVISOR_DEFAULT
#        define PLOOPY_SCROLL_DIVISOR_DEFAULT 1
#    endif
#endif
#ifndef PLOOPY_SCROLL_DIVISOR_DEFAULT_IDX
#    define PLOOPY_SCROLL_DIVISOR_DEFAULT_IDX 0
#endif

keyboard_config_t keyboard_config;
uint16_t          dpi_array[] = PLOOPY_DPI_OPTIONS;
#define DPI_OPTION_SIZE (sizeof(dpi_array) / sizeof(uint16_t))
float             scroll_divisors[] = PLOOPY_SCROLL_DIVISORS;
#define NUM_SCROLL_DIVISORS (sizeof(scroll_divisors) / sizeof(float))

// TODO: Implement libinput profiles
// https://wayland.freedesktop.org/libinput/doc/latest/pointer-acceleration.html
// Compile time accel selection
// Valid options are ACC_NONE, ACC_LINEAR, ACC_CUSTOM, ACC_QUADRATIC

// Trackball State
bool     is_drag_scroll    = false;
#if !PLOOPY_DRAGSCROLL_MOMENTARY && PLOOPY_DRAGSCROLL_ANY_MOUSE_KEYCODE_TOGGLES_OFF
uint16_t last_keycode_while_in_drag_scroll = KC_NO;
#endif

float scroll_accumulated_h = 0;
float scroll_accumulated_v = 0;

report_mouse_t pointing_device_task_kb(report_mouse_t mouse_report) {
    if (is_drag_scroll) {
        // Calculate and accumulate scroll values based on mouse movement and divisors
        float divisor = scroll_divisors[keyboard_config.scroll_divisor_idx];
        scroll_accumulated_h += (float)mouse_report.x / divisor;
        scroll_accumulated_v += (float)mouse_report.y / divisor;

        // Assign integer parts of accumulated scroll values to the mouse report
#if PLOOPY_DRAGSCROLL_INVERT
        // Invert scroll direction
        mouse_report.h = -(int8_t)scroll_accumulated_h;
        mouse_report.v = -(int8_t)scroll_accumulated_v;
#else
        mouse_report.h = (int8_t)scroll_accumulated_h;
        mouse_report.v = (int8_t)scroll_accumulated_v;
#endif

        // Update accumulated scroll values by subtracting the integer parts
        scroll_accumulated_h -= (int8_t)scroll_accumulated_h;
        scroll_accumulated_v -= (int8_t)scroll_accumulated_v;

        // Clear the X and Y values of the mouse report
        mouse_report.x = 0;
        mouse_report.y = 0;
    }

    return pointing_device_task_user(mouse_report);
}

void adjust_cpi_for_drag_scroll(void) {
    pointing_device_set_cpi(is_drag_scroll ? PLOOPY_DRAGSCROLL_DPI : dpi_array[keyboard_config.dpi_config]);
}

bool process_record_kb(uint16_t keycode, keyrecord_t* record) {
#if !PLOOPY_DRAGSCROLL_MOMENTARY && PLOOPY_DRAGSCROLL_ANY_MOUSE_KEYCODE_TOGGLES_OFF
    if (is_drag_scroll && record->event.pressed && IS_MOUSE_KEYCODE(keycode)) {
        last_keycode_while_in_drag_scroll = keycode;
        is_drag_scroll ^= 1;
        adjust_cpi_for_drag_scroll();
        return false;
    }
    if (keycode == last_keycode_while_in_drag_scroll && !record->event.pressed) {
        last_keycode_while_in_drag_scroll = KC_NO;
        return false;
    }
#endif

    if (!process_record_user(keycode, record)) {
        return false;
    }

    if (record->event.pressed) {
        uint8_t old_dpi_config = keyboard_config.dpi_config;
        if (keycode == CYCLE_DPI) {
            keyboard_config.dpi_config = (keyboard_config.dpi_config + 1) % DPI_OPTION_SIZE;
        } else if (keycode == DPI_1) {
            keyboard_config.dpi_config = 0;
        } else if (keycode == DPI_2) {
            keyboard_config.dpi_config = 1;
        } else if (keycode == DPI_3) {
            keyboard_config.dpi_config = 2;
        } else if (keycode == DPI_4) {
            keyboard_config.dpi_config = 3;
        }

        uint8_t old_scroll_divisor_idx = keyboard_config.scroll_divisor_idx;
        if (keycode == SCROLL_DIVISOR_1) {
            keyboard_config.scroll_divisor_idx = 0;
        } else if (keycode == SCROLL_DIVISOR_2) {
            keyboard_config.scroll_divisor_idx = 1;
        } else if (keycode == SCROLL_DIVISOR_3) {
            keyboard_config.scroll_divisor_idx = 2;
        }

        if (old_dpi_config != keyboard_config.dpi_config ||
                old_scroll_divisor_idx != keyboard_config.scroll_divisor_idx) {
            eeconfig_update_kb(keyboard_config.raw);
            adjust_cpi_for_drag_scroll();
        }
    }


    if (keycode == DRAG_SCROLL) {
#if !PLOOPY_DRAGSCROLL_MOMENTARY
        if (record->event.pressed)
#endif
        {
            is_drag_scroll ^= 1;
        }
        adjust_cpi_for_drag_scroll();
    }

    return true;
}

// Hardware Setup
void keyboard_pre_init_kb(void) {
    // debug_enable  = true;
    // debug_matrix  = true;
    // debug_mouse   = true;
    // debug_encoder = true;

    /* Ground all output pins connected to ground. This provides additional
     * pathways to ground. If you're messing with this, know this: driving ANY
     * of these pins high will cause a short. On the MCU. Ka-blooey.
     */
    const pin_t unused_pins[] = { GP1, GP3, GP4, GP6, GP8, GP10, GP14, GP16,
        GP18, GP20, GP22, GP24, GP25, GP26, GP27, GP28, GP29 };

    for (uint8_t i = 0; i < (sizeof(unused_pins) / sizeof(pin_t)); i++) {
        setPinOutput(unused_pins[i]);
        writePinLow(unused_pins[i]);
    }

    keyboard_pre_init_user();
}

void pointing_device_init_kb(void) {
    pointing_device_set_cpi(dpi_array[keyboard_config.dpi_config]);
}

void eeconfig_init_kb(void) {
    keyboard_config.dpi_config = PLOOPY_DPI_DEFAULT;
    keyboard_config.scroll_divisor_idx = PLOOPY_SCROLL_DIVISOR_DEFAULT_IDX;
    eeconfig_update_kb(keyboard_config.raw);
    eeconfig_init_user();
}

void matrix_init_kb(void) {
    // is safe to just read DPI setting since matrix init
    // comes before pointing device init.
    keyboard_config.raw = eeconfig_read_kb();
    if (keyboard_config.dpi_config >= DPI_OPTION_SIZE ||
            keyboard_config.scroll_divisor_idx >= NUM_SCROLL_DIVISORS) {
        eeconfig_init_kb();
    }
    matrix_init_user();
}
