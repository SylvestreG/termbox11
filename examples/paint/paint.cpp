#include "termbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int curCol = 0;
static int curRune = 0;
static struct tb_cell *backbuf;
static int bbw = 0, bbh = 0;

static const uint32_t runes[] = {
    0x20,   // ' '
    0x2591, // '░'
    0x2592, // '▒'
    0x2593, // '▓'
    0x2588, // '█'
};

#define len(a) (sizeof(a) / sizeof(a[0]))

static const uint16_t colors[] = {
    TB_BLACK, TB_RED,     TB_GREEN, TB_YELLOW,
    TB_BLUE,  TB_MAGENTA, TB_CYAN,  TB_WHITE,
};

void updateAndDrawButtons(int *current, int x, int y, int mx, int my, int n,
                          void (*attrFunc)(int, uint32_t *, uint16_t *,
                                           uint16_t *)) {
  int lx = x;
  int ly = y;
  for (int i = 0; i < n; i++) {
    if (lx <= mx && mx <= lx + 3 && ly <= my && my <= ly + 1) {
      *current = i;
    }
    uint32_t r;
    uint16_t fg, bg;
    (*attrFunc)(i, &r, &fg, &bg);
    tb_change_cell(lx + 0, ly + 0, r, fg, bg);
    tb_change_cell(lx + 1, ly + 0, r, fg, bg);
    tb_change_cell(lx + 2, ly + 0, r, fg, bg);
    tb_change_cell(lx + 3, ly + 0, r, fg, bg);
    tb_change_cell(lx + 0, ly + 1, r, fg, bg);
    tb_change_cell(lx + 1, ly + 1, r, fg, bg);
    tb_change_cell(lx + 2, ly + 1, r, fg, bg);
    tb_change_cell(lx + 3, ly + 1, r, fg, bg);
    lx += 4;
  }
  lx = x;
  ly = y;
  for (int i = 0; i < n; i++) {
    if (*current == i) {
      uint16_t fg = TB_RED | TB_BOLD;
      uint16_t bg = TB_DEFAULT;
      tb_change_cell(lx + 0, ly + 2, '^', fg, bg);
      tb_change_cell(lx + 1, ly + 2, '^', fg, bg);
      tb_change_cell(lx + 2, ly + 2, '^', fg, bg);
      tb_change_cell(lx + 3, ly + 2, '^', fg, bg);
    }
    lx += 4;
  }
}

void runeAttrFunc(int i, uint32_t *r, uint16_t *fg, uint16_t *bg) {
  *r = runes[i];
  *fg = TB_DEFAULT;
  *bg = TB_DEFAULT;
}

void colorAttrFunc(int i, uint32_t *r, uint16_t *fg, uint16_t *bg) {
  *r = ' ';
  *fg = TB_DEFAULT;
  *bg = colors[i];
}

void updateAndRedrawAll(termbox11 &tb, int mx, int my) {
  tb.clear();
  if (mx != -1 && my != -1) {
    backbuf[bbw * my + mx].ch = runes[curRune];
    backbuf[bbw * my + mx].fg = colors[curCol];
  }
  memcpy(tb_cell_buffer(), backbuf, sizeof(struct tb_cell) * bbw * bbh);
  int h = tb.height();
  updateAndDrawButtons(&curRune, 0, 0, mx, my, len(runes), runeAttrFunc);
  updateAndDrawButtons(&curCol, 0, h - 3, mx, my, len(colors), colorAttrFunc);
  tb.present();
}

void reallocBackBuffer(int w, int h) {
  bbw = w;
  bbh = h;
  if (backbuf)
    free(backbuf);
  backbuf = (struct tb_cell *)calloc(sizeof(struct tb_cell), w * h);
}



int main(int argv, char **argc) {
  (void)argc;
  (void)argv;
  try {
    auto tb = termbox11();

  tb_select_input_mode({.escaped = true, .mouse = true});
  int w = tb.width();
  int h = tb.height();
  reallocBackBuffer(w, h);
  updateAndRedrawAll(tb, -1, -1);
  for (;;) {
    struct tb_event ev;
    int mx = -1;
    int my = -1;
    event_type t = tb.poll_event(&ev);
    if (t == event_type::error) {
      fprintf(stderr, "termbox poll event error\n");
      return -1;
    }

    switch (t) {
    case event_type::key:
      if (ev.key == key_code::esc) {
        return 0;
      }
      break;
    case event_type::mouse:
      if (ev.key == key_code::mouse_left) {
        mx = ev.x;
        my = ev.y;
      }
      break;
    case event_type::resize:
      reallocBackBuffer(ev.w, ev.h);
      break;
    default:
      break;
    }
    updateAndRedrawAll(tb, mx, my);
  }
  } catch (std::exception const& e) {
    fprintf(stderr, "termbox init failed, code: %s\n", e.what());
    return -1;
  }
}
