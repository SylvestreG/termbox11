#include "termbox.h"
#include <stdio.h>
#include <string.h>

static const char chars[] = "nnnnnnnnnbbbbbbbbbuuuuuuuuuBBBBBBBBB";

static const uint16_t all_attrs[] = {
    0,
    TB_BOLD,
    TB_UNDERLINE,
    TB_BOLD | TB_UNDERLINE,
};

static int next_char(int current) {
  current++;
  if (!chars[current])
    current = 0;
  return current;
}

static void draw_line(int x, int y, uint16_t bg) {
  int a, c;
  int current_char = 0;
  for (a = 0; a < 4; a++) {
    for (c = TB_DEFAULT; c <= TB_WHITE; c++) {
      uint16_t fg = all_attrs[a] | c;
      tb_change_cell(x, y, chars[current_char], fg, bg);
      current_char = next_char(current_char);
      x++;
    }
  }
}

static void print_combinations_table(int sx, int sy, const uint16_t *attrs,
                                     int attrs_n) {
  int i, c;
  for (i = 0; i < attrs_n; i++) {
    for (c = TB_DEFAULT; c <= TB_WHITE; c++) {
      uint16_t bg = attrs[i] | c;
      draw_line(sx, sy, bg);
      sy++;
    }
  }
}

static void draw_all(termbox11 &tb) {
  tb.clear();

  tb.select_output_mode(output_mode::normal);
  static const uint16_t col1[] = {0, TB_BOLD};
  static const uint16_t col2[] = {TB_REVERSE};
  print_combinations_table(1, 1, col1, 2);
  print_combinations_table(2 + strlen(chars), 1, col2, 1);
  tb.present();

  tb.select_output_mode(output_mode::grayscale);
  int c, x, y;
  for (x = 0, y = 23; x < 24; ++x) {
    tb_change_cell(x, y, '@', x, 0);
    tb_change_cell(x + 25, y, ' ', 0, x);
  }
  tb.present();

  tb.select_output_mode(output_mode::mode216);
  y++;
  for (c = 0, x = 0; c < 216; ++c, ++x) {
    if (!(x % 24)) {
      x = 0;
      ++y;
    }
    tb_change_cell(x, y, '@', c, 0);
    tb_change_cell(x + 25, y, ' ', 0, c);
  }
  tb.present();

  tb.select_output_mode(output_mode::mode256);
  y++;
  for (c = 0, x = 0; c < 256; ++c, ++x) {
    if (!(x % 24)) {
      x = 0;
      ++y;
    }
    tb_change_cell(x, y, '+', c | ((y & 1) ? TB_UNDERLINE : 0), 0);
    tb_change_cell(x + 25, y, ' ', 0, c);
  }
  tb.present();
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  try {
    auto tb = termbox11();

    draw_all(tb);

    struct tb_event ev;
    while (tb.poll_event(&ev) != event_type::none) {
      switch (ev.type) {
      case event_type::key:
        switch (ev.key) {
        case key_code::esc:
          goto done;
          break;
        default:
          break;
        }
        break;
      case event_type::resize:
        draw_all(tb);
        break;
      default:
        break;
      }
    }
  done:

    return 0;
  } catch (std::exception const &ex) {
    fprintf(stderr, "tb_init() failed with error code %s\n", ex.what());
    return 1;
  }
}
