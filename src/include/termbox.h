#ifndef __TERMBOX_H__
#define __TERMBOX_H__

#include <cstdint>

/* Key constants. See also struct tb_event's key field.
 *
 * These are a safe subset of terminfo keys, which exist on all popular
 * terminals. Termbox uses only them to stay truly portable.
 */
enum class key_code : std::uint16_t {
  f1 = 0xFFFF,
  f2 = 0xFFFF - 1,
  f3 = 0xFFFF - 2,
  f4 = 0xFFFF - 3,
  f5 = 0xFFFF - 4,
  f6 = 0xFFFF - 5,
  f7 = 0xFFFF - 6,
  f8 = 0xFFFF - 7,
  f9 = 0xFFFF - 8,
  f10 = 0xFFFF - 9,
  f11 = 0xFFFF - 10,
  f12 = 0xFFFF - 11,
  insert = 0xFFFF - 12,
  del = 0xFFFF - 13,
  home = 0xFFFF - 14,
  end = 0xFFFF - 15,
  pg_up = 0xFFFF - 16,
  pg_down = 0xFFFF - 17,
  arrow_up = 0xFFFF - 18,
  arrow_down = 0xFFFF - 19,
  arrow_left = 0xFFFF - 20,
  arrow_right = 0xFFFF - 21,
  mouse_left = 0xFFFF - 22,
  mouse_right = 0xFFFF - 23,
  mouse_middle = 0xFFFF - 24,
  mouse_release = 0xFFFF - 25,
  mouse_wheel_up = 0xFFFF - 26,
  mouse_wheel_down = 0xFFFF - 27,

  /* These are all ASCII code points below SPACE character and a BACKSPACE key.
   */
  ctrl_tilde = 0x00,
  /* clash with 'ctrl_tilde' */
  ctrl_2 = 0x00,
  ctrl_a = 0x01,
  ctrl_b = 0x02,
  ctrl_c = 0x03,
  ctrl_d = 0x04,
  ctrl_e = 0x05,
  ctrl_f = 0x06,
  ctrl_g = 0x07,
  backspace = 0x08,
  /* clash with 'ctrl_backspace' */
  ctrl_h = 0x08,
  tab = 0x09,
  /* clash with 'tab' */
  ctrl_i = 0x09,
  ctrl_j = 0x0a,
  ctrl_k = 0x0b,
  ctrl_l = 0x0c,
  enter = 0x0d,
  /* clash with 'enter' */
  ctrl_m = 0x0d,
  ctrl_n = 0x0e,
  ctrl_o = 0x0f,
  ctrl_p = 0x10,
  ctrl_q = 0x11,
  ctrl_r = 0x12,
  ctrl_s = 0x13,
  ctrl_t = 0x14,
  ctrl_u = 0x15,
  ctrl_v = 0x16,
  ctrl_w = 0x17,
  ctrl_x = 0x18,
  ctrl_y = 0x19,
  ctrl_z = 0x1a,
  esc = 0x1b,
  /* clash with 'esc' */
  ctrl_lsq_bracket = 0x1b,
  /* clash with 'esc' */
  ctrl_3 = 0x1b,
  ctrl_4 = 0x1c,
  /* clash with 'ctrl_4' */
  ctrl_backslash = 0x1c,
  ctrl_5 = 0x1d,
  /* clash with 'ctrl_5' */
  ctrl_rsq_bracket = 0x1d,
  ctrl_6 = 0x1e,
  ctrl_7 = 0x1f,
  /* clash with 'ctrl_7' */
  ctrl_slash = 0x1f,
  /* clash with 'ctrl_7' */
  ctrl_underscore = 0x1f,
  space = 0x20,
  backspace2 = 0x7f,
  /* clash with 'backspace2' */
  ctrl_8 = 0x7f,
};
/* These are non-existing ones.
 *
 * #define TB_KEY_CTRL_1 clash with '1'
 * #define TB_KEY_CTRL_9 clash with '9'
 * #define TB_KEY_CTRL_0 clash with '0'
 */

/*
 * Alt modifier constant, see tb_event.mod field and tb_select_input_mode
 * function. Mouse-motion modifier
 */
enum class modifiers : std::uint8_t {
  none = 0x00,
  alt = 0x01,
  motion = 0x02,
  both = 0x03
};

modifiers &operator|=(modifiers &lhs, modifiers rhs);

/* Colors (see struct tb_cell's fg and bg fields). */
#define TB_DEFAULT 0x00
#define TB_BLACK 0x01
#define TB_RED 0x02
#define TB_GREEN 0x03
#define TB_YELLOW 0x04
#define TB_BLUE 0x05
#define TB_MAGENTA 0x06
#define TB_CYAN 0x07
#define TB_WHITE 0x08

/* Attributes, it is possible to use multiple attributes by combining them
 * using bitwise OR ('|'). Although, colors cannot be combined. But you can
 * combine attributes and a single color. See also struct tb_cell's fg and bg
 * fields.
 */
#define TB_BOLD 0x0100
#define TB_UNDERLINE 0x0200
#define TB_REVERSE 0x0400

/* A cell, single conceptual entity on the terminal screen. The terminal screen
 * is basically a 2d array of cells. It has the following fields:
 *  - 'ch' is a unicode character
 *  - 'fg' foreground color and attributes
 *  - 'bg' background color and attributes
 */
struct tb_cell {
  uint32_t ch;
  uint16_t fg;
  uint16_t bg;
};

enum class event_type {
  none,
  key,
  resize,
  mouse,
  error
};

/* An event, single interaction from the user. The 'mod' and 'ch' fields are
 * valid if 'type' is TB_EVENT_KEY. The 'w' and 'h' fields are valid if 'type'
 * is TB_EVENT_RESIZE. The 'x' and 'y' fields are valid if 'type' is
 * TB_EVENT_MOUSE. The 'key' field is valid if 'type' is either TB_EVENT_KEY
 * or TB_EVENT_MOUSE. The fields 'key' and 'ch' are mutually exclusive; only
 * one of them can be non-zero at a time.
 */
struct tb_event {
  event_type type;
  modifiers mod;  /* modifiers to either 'key' or 'ch' below */
  key_code key; /* one of the TB_KEY_* constants */
  uint32_t ch;  /* unicode character */
  int32_t w;
  int32_t h;
  int32_t x;
  int32_t y;
};

/* Error codes returned by tb_init(). All of them are self-explanatory, except
 * the pipe trap error. Termbox uses unix pipes in order to deliver a message
 * from a signal handler (SIGWINCH) to the main event reading loop. Honestly in
 * most cases you should just check the returned code as < 0.
 */
#define TB_EUNSUPPORTED_TERMINAL -1
#define TB_EFAILED_TO_OPEN_TTY -2
#define TB_EPIPE_TRAP_ERROR -3

/* Initializes the termbox library. This function should be called before any
 * other functions. Function tb_init is same as tb_init_file("/dev/tty").
 * After successful initialization, the library must be
 * finalized using the tb_shutdown() function.
 */
int tb_init(void);
int tb_init_file(const char *name);
int tb_init_fd(int inout);
void tb_shutdown(void);

/* Returns the size of the internal back buffer (which is the same as
 * terminal's window size in characters). The internal buffer can be resized
 * after tb_clear() or tb_present() function calls. Both dimensions have an
 * unspecified negative value when called before tb_init() or after
 * tb_shutdown().
 */
int tb_width(void);
int tb_height(void);

/* Clears the internal back buffer using TB_DEFAULT color or the
 * color/attributes set by tb_set_clear_attributes() function.
 */
void tb_clear(void);
void tb_set_clear_attributes(uint16_t fg, uint16_t bg);

/* Synchronizes the internal back buffer with the terminal. */
void tb_present(void);

#define TB_HIDE_CURSOR -1

/* Sets the position of the cursor. Upper-left character is (0, 0). If you pass
 * TB_HIDE_CURSOR as both coordinates, then the cursor will be hidden. Cursor
 * is hidden by default.
 */
void tb_set_cursor(int cx, int cy);

/* Changes cell's parameters in the internal back buffer at the specified
 * position.
 */
void tb_put_cell(int x, int y, const struct tb_cell *cell);
void tb_change_cell(int x, int y, uint32_t ch, uint16_t fg,
                              uint16_t bg);

/* Copies the buffer from 'cells' at the specified position, assuming the
 * buffer is a two-dimensional array of size ('w' x 'h'), represented as a
 * one-dimensional buffer containing lines of cells starting from the top.
 *
 * (DEPRECATED: use tb_cell_buffer() instead and copy memory on your own)
 */
void tb_blit(int x, int y, int w, int h, const struct tb_cell *cells);

/* Returns a pointer to internal cell back buffer. You can get its dimensions
 * using tb_width() and tb_height() functions. The pointer stays valid as long
 * as no tb_clear() and tb_present() calls are made. The buffer is
 * one-dimensional buffer containing lines of cells starting from the top.
 */
struct tb_cell *tb_cell_buffer(void);

#define TB_INPUT_CURRENT 0 /* 000 */
#define TB_INPUT_ESC 1 /* 001 */
#define TB_INPUT_ALT 2 /* 010 */
#define TB_INPUT_MOUSE 4 /* 100 */

/* Sets the termbox input mode. Termbox has two input modes:
 * 1. Esc input mode.
 *    When ESC sequence is in the buffer and it doesn't match any known
 *    ESC sequence => ESC means TB_KEY_ESC.
 * 2. Alt input mode.
 *    When ESC sequence is in the buffer and it doesn't match any known
 *    sequence => ESC enables TB_MOD_ALT modifier for the next keyboard event.
 *
 * You can also apply TB_INPUT_MOUSE via bitwise OR operation to either of the
 * modes (e.g. TB_INPUT_ESC | TB_INPUT_MOUSE). If none of the main two modes
 * were set, but the mouse mode was, TB_INPUT_ESC mode is used. If for some
 * reason you've decided to use (TB_INPUT_ESC | TB_INPUT_ALT) combination, it
 * will behave as if only TB_INPUT_ESC was selected.
 *
 * If 'mode' is TB_INPUT_CURRENT, it returns the current input mode.
 *
 * Default termbox input mode is TB_INPUT_ESC.
 */
int tb_select_input_mode(int mode);

enum class output_mode {
  current,
  normal,
  mode256,
  mode216,
  grayscale
};

/* Sets the termbox output mode. Termbox has three output options:
 * 1. TB_OUTPUT_NORMAL     => [1..8]
 *    This mode provides 8 different colors:
 *      black, red, green, yellow, blue, magenta, cyan, white
 *    Shortcut: TB_BLACK, TB_RED, ...
 *    Attributes: TB_BOLD, TB_UNDERLINE, TB_REVERSE
 *
 *    Example usage:
 *        tb_change_cell(x, y, '@', TB_BLACK | TB_BOLD, TB_RED);
 *
 * 2. TB_OUTPUT_256        => [0..256]
 *    In this mode you can leverage the 256 terminal mode:
 *    0x00 - 0x07: the 8 colors as in TB_OUTPUT_NORMAL
 *    0x08 - 0x0f: TB_* | TB_BOLD
 *    0x10 - 0xe7: 216 different colors
 *    0xe8 - 0xff: 24 different shades of grey
 *
 *    Example usage:
 *        tb_change_cell(x, y, '@', 184, 240);
 *        tb_change_cell(x, y, '@', 0xb8, 0xf0);
 *
 * 3. TB_OUTPUT_216        => [0..216]
 *    This mode supports the 3rd range of the 256 mode only.
 *    But you don't need to provide an offset.
 *
 * 4. TB_OUTPUT_GRAYSCALE  => [0..23]
 *    This mode supports the 4th range of the 256 mode only.
 *    But you dont need to provide an offset.
 *
 * Execute build/src/demo/output to see its impact on your terminal.
 *
 * If 'mode' is TB_OUTPUT_CURRENT, it returns the current output mode.
 *
 * Default termbox output mode is TB_OUTPUT_NORMAL.
 */
output_mode tb_select_output_mode(output_mode mode);

/* Wait for an event up to 'timeout' milliseconds and fill the 'event'
 * structure with it, when the event is available. Returns the type of the
 * event (one of TB_EVENT_* constants) or -1 if there was an error or 0 in case
 * there were no event during 'timeout' period.
 */
event_type tb_peek_event(struct tb_event *event, int timeout);

/* Wait for an event forever and fill the 'event' structure with it, when the
 * event is available. Returns the type of the event (one of TB_EVENT_*
 * constants) or -1 if there was an error.
 */
event_type tb_poll_event(struct tb_event *event);

/* Utility utf8 functions. */
#define TB_EOF -1
int tb_utf8_char_length(char c);
int tb_utf8_char_to_unicode(uint32_t *out, const char *c);
int tb_utf8_unicode_to_char(char *out, uint32_t c);

#endif // __TERMBOX_H__
