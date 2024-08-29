#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <utils/queue.h>
#include <kernel/keyboard.h>
#include <kernel/ps2controller.h>
#include <kernel/apic.h>
#include <kernel/idt.h>
#include <kernel/task.h>
#include <kernel/file.h>
#include <kernel/stdio_handler.h>

static queue_t *command_queue;

enum {
        KB_AVAILABLE,
        CMD_ACKNOW,
        CMD_RESPONSE,
};
static uint8_t status = KB_AVAILABLE;

static struct {
        uint8_t shift_flag;
        uint8_t ctrl_flag;
        uint8_t alt_flag;
        uint8_t altgr_flag;
        uint8_t gui_flag; /* (WINDOWS) win key, (MAC) cmd key */
        uint8_t fn_flag;
} kb_flags;

void
keyboard_send_cmd(uint8_t command)
{
        status = CMD_ACKNOW;
        outb(command, PS2_DATAPORT);
}

void
keyboard_send_next(void)
{
        if (status == KB_AVAILABLE && command_queue->size > 0) {
                uint32_t next_command = (uint32_t) queue_top(command_queue);

                keyboard_send_cmd((uint8_t) next_command);
        }
}

void
keyboard_queuecommand(uint8_t command)
{
        uint32_t cmd = command;
        queue_push(command_queue, (void*) cmd);
        
        if (command_queue->size == 1)
                keyboard_send_cmd(command);
}

static void
keyboard_handlequeue(uint8_t response)
{
        static uint8_t repetition = 0;

        if (response == 0xFA) {
                uint8_t command = (uint8_t)(uint32_t) queue_top(command_queue);
                queue_pop(command_queue);
                repetition = 0;

                if (command == 0xFF)
                        status = CMD_RESPONSE;
                else
                        status = KB_AVAILABLE;
                
        } else if (response == 0xFE) {
                ++repetition;

                if (repetition > 3) {
                        kprintf("keyboard doesn't support this command or hardware failure\n");
                        queue_pop(command_queue);
                        repetition = 0;
                }

                status = KB_AVAILABLE;
        }

        if (status == KB_AVAILABLE)
                keyboard_send_next();
}

static void
keyboard_handleresponse(uint8_t response)
{
        switch (response) {
        case 0xAA:
                kprintf("KEYBOARD self test PASSED\n");
                break;
                
        case 0xEE:
                kprintf("KEYBOARD echo\n");
                break;
                
        case 0xFC:
        case 0xFD:
                kprintf("KEYBOARD self test FAILED\n");
                break;
                
        case 0x00:
        case 0xFF:
                kprintf("KEYBOARD detection error or internal buffer overrun\n");
                break;
        }

        status = KB_AVAILABLE;
        keyboard_send_next();
}

static uint8_t keycodes[8];
static uint32_t len = 0;
static uint32_t scancode_table[] = {
        0, KEY_F9, 0, KEY_F5, KEY_F3, KEY_F1, KEY_F2, KEY_F12, 0,    /* 0x0-0x8 */
        KEY_F10, KEY_F8, KEY_F6, KEY_F4, KEY_TAB, KEY_TILDE, 0, 0,   /* 0x9-0x10 */
        KEY_LALT, KEY_LSHIFT, 0, KEY_LCTRL, KEY_Q, KEY_1, 0, 0, 0,   /* 0x11-0x19 */
        KEY_Z, KEY_S, KEY_A, KEY_W, KEY_2, 0, 0, KEY_C, KEY_X,       /* 0x1A-0x22 */
        KEY_D, KEY_E, KEY_4, KEY_3, 0, 0, KEY_SPACE, KEY_V, KEY_F,   /* 0x23-0x2B */
        KEY_T, KEY_R, KEY_5, 0, 0, KEY_N, KEY_B, KEY_H, KEY_G,       /* 0x2C-0x34 */
        KEY_Y, KEY_6, 0, 0, 0, KEY_M, KEY_J, KEY_U, KEY_7, KEY_8,    /* 0x35-0x3E */
        0, 0, KEY_COMMA, KEY_K, KEY_I, KEY_O, KEY_0, KEY_9, 0,       /* 0x3F-0x47 */
        0, KEY_PERIOD, KEY_SLASH, KEY_L, KEY_SEMICOLON, KEY_P,       /* 0x48-0x4D */
        KEY_DASH, 0, 0, 0, KEY_APOS, 0, KEY_LSBRACKET, KEY_EQUAL,    /* 0x4E-0x55 */
        0, 0, KEY_CAPSLOCK, KEY_RSHIFT, KEY_ENTER, KEY_RSBRACKET,    /* 0x56-0x5B */
        0, KEY_BACKSLASH, 0, 0, 0, 0, 0, 0, 0, 0, KEY_BACKSPACE,     /* 0x5C-0x66 */
        0, 0, KEY_NUMPAD1, 0, KEY_NUMPAD4, KEY_NUMPAD7, 0, 0, 0,     /* 0x67-0x6F */
        KEY_NUMPAD0, KEY_NUMPAD_PERIOD, KEY_NUMPAD2, KEY_NUMPAD5,    /* 0x70-0x73 */
        KEY_NUMPAD6, KEY_NUMPAD8, KEY_ESCAPE, KEY_NUMLOCK, KEY_F11,  /* 0x74-0x78 */
        KEY_NUMPAD_PLUS, KEY_NUMPAD3, KEY_NUMPAD_DASH,               /* 0x79-0x7B */
        KEY_NUMPAD_STAR, KEY_NUMPAD9, KEY_SCROLLLOCK, 0, 0, 0, 0,    /* 0x7C-0x82 */
        KEY_F7,                                                      /* 0x83 */
};

/* the first entry is 0x10, so there should be an offset */
static uint32_t alt_scancode_table[] = {
        KEY_SEARCH, KEY_RALT, 0, 0, KEY_RCTRL, KEY_PREVTRACK, 0, 0,  /* 0x10-0x17 */
        KEY_FAV, 0, 0, 0, 0, 0, 0, KEY_LGUI, KEY_REFRESH, KEY_DVOL,  /* 0x18-0x21 */
        0, KEY_MUTE, 0, 0, 0, KEY_RGUI, KEY_WSTOP, 0, 0, KEY_CALC,   /* 0x22-0x2B */
        0, 0, 0, KEY_APPS, KEY_WFORWARD, 0, KEY_UVOL, 0, KEY_PLAY,   /* 0x2C-0x34 */
        0, 0, KEY_POWER, KEY_WBACK, 0, KEY_WHOME, KEY_STOP, 0, 0, 0, /* 0x35-0x3E */
        KEY_SLEEP, KEY_MYPC, 0, 0, 0, 0, 0, 0, 0, KEY_EMAIL, 0,      /* 0x3F-0x49 */
        KEY_NUMPAD_SLASH, 0, 0, KEY_NEXTTRACK, 0, 0, KEY_MEDIA, 0,   /* 0x4A-0x51 */
        0, 0, 0, 0, 0, 0, 0, 0, KEY_NUMPAD_ENTER, 0, 0, 0, KEY_WAKE, /* 0x52-0x5E */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KEY_END, 0, KEY_LEFT, KEY_HOME,/* 0x5F-0x6C */
        0, 0, 0, KEY_INSERT, KEY_DEL, KEY_DOWN, 0, KEY_RIGHT, KEY_UP,/* 0x6D-0x75 */
        0, 0, 0, 0, KEY_PDOWN, 0, 0, KEY_PUP,                        /* 0x76-0x7D */
};

static uint32_t
keyboard_scancode_to_keycode(void)
{
        uint32_t ret = 0;
       
        switch (len) {
        case 1:
                ret = scancode_table[keycodes[0]]; 
                break;
        case 2:
                if (keycodes[0] == 0xE0)
                        ret = alt_scancode_table[keycodes[1] - 0x10];
                else if (keycodes[0] == 0xF0)
                        ret = scancode_table[keycodes[1]] | KB_RELEASE_FLAG;
                break;

        case 3:
                if (keycodes[0] == 0xE0 && keycodes[1] == 0xF0)
                        ret = alt_scancode_table[keycodes[2] - 0x10] | KB_RELEASE_FLAG;
                break;

        case 4: {
                static uint8_t prtsc_codes[] = {0xE0, 0x12, 0xE0, 0x7C};
                if (!memcmp(keycodes, prtsc_codes, sizeof(prtsc_codes)))
                        ret = KEY_PRTSC;
                break;
        }

        case 6: {
                static uint8_t prtsc_codes[] = {0xE0, 0xF0, 0x7C, 0xE0, 0xF0, 0x12};
                if (!memcmp(keycodes, prtsc_codes, sizeof(prtsc_codes)))
                        ret = KEY_PRTSC | KB_RELEASE_FLAG;
                break;
        }

        case 8: {
                static uint8_t pause_codes[] = {0xE1, 0x14, 0x77, 0xE1,
                                                0xF0, 0x14, 0xF0, 0x77};
                if (!memcmp(keycodes, pause_codes, sizeof(pause_codes)))
                        ret = KEY_PAUSE;
                break;
        }
        }

        len = 0;
        return ret;
}

static void
keyboard_handle_shift(int is_release) 
{
        static int hold = 0;

        if (hold == is_release) {
                kb_flags.shift_flag = 1 - kb_flags.shift_flag;
                hold = 1 - hold;
        }
}

static void
keyboard_handle_keycode(uint32_t code)
{
        int is_release = code >> 31;
        uint32_t flagless_code = code & ~KB_RELEASE_FLAG;
        switch (flagless_code) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
                keyboard_handle_shift(is_release);
                break;

        case KEY_CAPSLOCK:
                if (!is_release)
                        kb_flags.shift_flag = 1 - kb_flags.shift_flag;
                break;

        case KEY_LCTRL:
        case KEY_RCTRL:
                kb_flags.ctrl_flag = (is_release) ? 0 : 1;
                break;
                
        case KEY_LALT:
                kb_flags.alt_flag = (is_release) ? 0 : 1;
                break;

        case KEY_RALT:
                kb_flags.altgr_flag = (is_release) ? 0 : 1;
                break;

        case KEY_LGUI:
                kb_flags.gui_flag = (is_release) ? 0 : 1;
                break;

        default:
                if (is_release)
                        break;

                if (kb_flags.shift_flag) flagless_code |= KB_SHIFT_FLAG;
                if (kb_flags.ctrl_flag) flagless_code |= KB_CTRL_FLAG;
                if (kb_flags.alt_flag) flagless_code |= KB_ALT_FLAG;
                if (kb_flags.altgr_flag) flagless_code |= KB_ALTGR_FLAG;
                if (kb_flags.gui_flag) flagless_code |= KB_GUI_FLAG;

                kb_keysym_t keysym = {
                        .ascii = 0,
                        .flags = (flagless_code >> 16) & 0xFF,
                        .keysym = flagless_code & 0xFFFF,
                };

                stdin_send_code(keysym);
        }
}

// TODO: complete keyboard input 
static void
keyboard_scancode(uint8_t code)
{
        keycodes[len++] = code;
        
        if (code == 0xE0 || code == 0xF0 || code == 0xE1)
                return;

        /* PAUSE */
        if (code == 0x14) {
                if (len == 2 && keycodes[0] == 0xE1)
                        return;
                else if (len == 6 && keycodes[4] == 0xF0)
                        return;
        }

        if (code == 0x77 && len == 3 && keycodes[1] == 0x14)
                return;
                
        /* KEY_PRTSC */
        if (code == 0x12 && len == 2 && keycodes[0] == 0xE0)
                return;

        /* RELEASE KEY_PRTSC */
        if (code == 0x7C && len == 3 && keycodes[0] == 0xE0 && keycodes[1] == 0xF0)
                return;
        
        uint32_t keycode = keyboard_scancode_to_keycode();
        lapic_sendEOI();
        keyboard_handle_keycode(keycode);
}

__attribute__ ((interrupt))
static void
keyboard_handlecode(interrupt_frame_t *frame)
{
        (void) frame;
        
        uint8_t byte = inb(PS2_DATAPORT);
        
        switch (status) {
        case CMD_RESPONSE:
                keyboard_handleresponse(byte);
                break;
                
        case CMD_ACKNOW:
                keyboard_handlequeue(byte);
                break;

        case KB_AVAILABLE:
                keyboard_scancode(byte);
                lapic_sendEOI();
                return;
        }

        lapic_sendEOI();
}

void
keyboard_init(void)
{
        command_queue = queue_init(COMMAND_QUEUE_CAP);

        idt_create(idt_entries + IRQ_OFFSET + 1, (uintptr_t)keyboard_handlecode, 0x8E);
        ioapic_legacy_irq_activate(1);
        ioapic_irq_clear_mask(1);

        keyboard_queuecommand(0xFF);
        
        keyboard_queuecommand(0xED);
        keyboard_queuecommand(0x0);

        keyboard_queuecommand(0xF0);
        keyboard_queuecommand(0x2);
        
        keyboard_queuecommand(0xF4);
        
        kprintf("KEYBOARD setup SUCCESS\n");
}
