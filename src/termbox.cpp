#include "termbox.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

#include "term.inl"

#include "bytebuffer.inl"
#include "input.inl"

struct cellbuf {
  int width;
  int height;
  struct tb_cell *cells;
};

#define CELL(buf, x, y) (buf)->cells[(y) * (buf)->width + (x)]
#define IS_CURSOR_HIDDEN(cx, cy) (cx == -1 || cy == -1)
#define LAST_COORD_INIT -1

static struct termios orig_tios;

static struct cellbuf back_buffer;
static struct cellbuf front_buffer;


static int inout;
static int winch_fds[2];

static int lastx = LAST_COORD_INIT;
static int lasty = LAST_COORD_INIT;
static int cursor_x = -1;
static int cursor_y = -1;

static uint16_t background = TB_DEFAULT;
static uint16_t foreground = TB_DEFAULT;

static void cellbuf_init(struct cellbuf *buf, int width, int height);
static void cellbuf_resize(struct cellbuf *buf, int width, int height);
static void cellbuf_clear(struct cellbuf *buf);
static void cellbuf_free(struct cellbuf *buf);

static void sigwinch_handler(int xxx);

/* may happen in a different thread */
modifiers &operator|=(modifiers &lhs, modifiers rhs) {
  switch (lhs) {
  case modifiers::both:
    break;
  case modifiers::none:
    lhs = rhs;
    break;
  case modifiers::alt:
    switch (rhs) {
    case modifiers::motion:
      lhs = modifiers::both;
      break;
    default:
      break;
    }
  case modifiers::motion:
    switch (rhs) {
    case modifiers::alt:
      lhs = modifiers::both;
    default:
      break;
    }
  }

  return lhs;
}

/* -------------------------------------------------------- */

void tb_put_cell(int x, int y, const struct tb_cell *cell) {
  if ((unsigned)x >= (unsigned)back_buffer.width)
    return;
  if ((unsigned)y >= (unsigned)back_buffer.height)
    return;
  CELL(&back_buffer, x, y) = *cell;
}

void tb_change_cell(int x, int y, uint32_t ch, uint16_t fg, uint16_t bg) {
  struct tb_cell c = {ch, fg, bg};
  tb_put_cell(x, y, &c);
}

void tb_blit(int x, int y, int w, int h, const struct tb_cell *cells) {
  if (x + w < 0 || x >= back_buffer.width)
    return;
  if (y + h < 0 || y >= back_buffer.height)
    return;
  int xo = 0, yo = 0, ww = w, hh = h;
  if (x < 0) {
    xo = -x;
    ww -= xo;
    x = 0;
  }
  if (y < 0) {
    yo = -y;
    hh -= yo;
    y = 0;
  }
  if (ww > back_buffer.width - x)
    ww = back_buffer.width - x;
  if (hh > back_buffer.height - y)
    hh = back_buffer.height - y;

  int sy;
  struct tb_cell *dst = &CELL(&back_buffer, x, y);
  const struct tb_cell *src = cells + yo * w + xo;
  size_t size = sizeof(struct tb_cell) * ww;

  for (sy = 0; sy < hh; ++sy) {
    memcpy(dst, src, size);
    dst += back_buffer.width;
    src += w;
  }
}

struct tb_cell *tb_cell_buffer(void) {
  return back_buffer.cells;
}

void tb_set_clear_attributes(uint16_t fg, uint16_t bg) {
  foreground = fg;
  background = bg;
}

/* -------------------------------------------------------- */

static int convertnum(uint32_t num, char *buf) {
  int i, l = 0;
  int ch;
  do {
    buf[l++] = '0' + (num % 10);
    num /= 10;
  } while (num);
  for (i = 0; i < l / 2; i++) {
    ch = buf[i];
    buf[i] = buf[l - 1 - i];
    buf[l - 1 - i] = ch;
  }
  return l;
}

#define WRITE_LITERAL(X) bytebuffer_append(&_output_buffer, (X), sizeof(X) - 1)
#define WRITE_INT(X)                                                           \
  bytebuffer_append(&_output_buffer, buf, convertnum((X), buf))

static void cellbuf_init(struct cellbuf *buf, int width, int height) {
  buf->cells =
      (struct tb_cell *)malloc(sizeof(struct tb_cell) * width * height);
  assert(buf->cells);
  buf->width = width;
  buf->height = height;
}

static void cellbuf_resize(struct cellbuf *buf, int width, int height) {
  if (buf->width == width && buf->height == height)
    return;

  int oldw = buf->width;
  int oldh = buf->height;
  struct tb_cell *oldcells = buf->cells;

  cellbuf_init(buf, width, height);
  cellbuf_clear(buf);

  int minw = (width < oldw) ? width : oldw;
  int minh = (height < oldh) ? height : oldh;
  int i;

  for (i = 0; i < minh; ++i) {
    struct tb_cell *csrc = oldcells + (i * oldw);
    struct tb_cell *cdst = buf->cells + (i * width);
    memcpy(cdst, csrc, sizeof(struct tb_cell) * minw);
  }

  free(oldcells);
}

static void cellbuf_clear(struct cellbuf *buf) {
  int i;
  int ncells = buf->width * buf->height;

  for (i = 0; i < ncells; ++i) {
    buf->cells[i].ch = ' ';
    buf->cells[i].fg = foreground;
    buf->cells[i].bg = background;
  }
}

static void cellbuf_free(struct cellbuf *buf) { free(buf->cells); }

static void get_term_size(int *w, int *h) {
  struct winsize sz;
  memset(&sz, 0, sizeof(sz));

  ioctl(inout, TIOCGWINSZ, &sz);

  if (w)
    *w = sz.ws_col;
  if (h)
    *h = sz.ws_row;
}

static void sigwinch_handler(int xxx) {
  (void)xxx;
  const int zzz = 1;
  write(winch_fds[1], &zzz, sizeof(int));
}

struct termbox_impl {
public:
  void update_term_size();
  void update_size();
  event_type wait_fill_event(struct tb_event *event, struct timeval *timeout);
  void write_cursor(int x, int y);
  void write_sgr(uint16_t fg, uint16_t bg);
  void send_attr(uint16_t fg, uint16_t bg);
  void send_char(int x, int y, uint32_t c);
  void send_clear(void);
  int read_up_to(int n);

private:
  size_t _w;
  size_t _h;
  bool _buffer_size_change_request;
  struct bytebuffer _output_buffer;
  struct bytebuffer _input_buffer;
  input_mode _inputmode{true, false, false};
  output_mode _outputmode{output_mode::normal};

  friend termbox11;
};

void termbox_impl::update_term_size() {
  struct winsize sz;
  memset(&sz, 0, sizeof(sz));

  ioctl(inout, TIOCGWINSZ, &sz);

  _w = sz.ws_col;
  _h = sz.ws_row;
}

void termbox_impl::update_size() {
    update_term_size();
    cellbuf_resize(&back_buffer, _w, _h);
    cellbuf_resize(&front_buffer, _w, _h);
    cellbuf_clear(&front_buffer);
    send_clear();
}
event_type termbox_impl::wait_fill_event(struct tb_event *event,
                                         struct timeval *timeout) {
#define ENOUGH_DATA_FOR_PARSING 64
  fd_set events;
  memset(event, 0, sizeof(struct tb_event));

  // try to extract event from input buffer, return on success
  event->type = event_type::key;
  if (extract_event(event, &_input_buffer, _inputmode))
    return event->type;

  // it looks like input buffer is incomplete, let's try the short path,
  // but first make sure there is enough space
  int n = read_up_to(ENOUGH_DATA_FOR_PARSING);
  if (n < 0)
    return event_type::error;
  if (n > 0 && extract_event(event, &_input_buffer, _inputmode))
    return event->type;

  // n == 0, or not enough data, let's go to select
  while (1) {
    FD_ZERO(&events);
    FD_SET(inout, &events);
    FD_SET(winch_fds[0], &events);
    int maxfd = (winch_fds[0] > inout) ? winch_fds[0] : inout;
    int result = select(maxfd + 1, &events, 0, 0, timeout);
    if (!result)
      return event_type::none;

    if (FD_ISSET(inout, &events)) {
      event->type = event_type::key;
      n = read_up_to(ENOUGH_DATA_FOR_PARSING);
      if (n < 0)
        return event_type::error;

      if (n == 0)
        continue;

      if (extract_event(event, &_input_buffer, _inputmode))
        return event->type;
    }
    if (FD_ISSET(winch_fds[0], &events)) {
      event->type = event_type::resize;
      int zzz = 0;
      read(winch_fds[0], &zzz, sizeof(int));
      _buffer_size_change_request = true;
      get_term_size(&event->w, &event->h);
      return event_type::resize;
    }
  }
}

void termbox_impl::write_cursor(int x, int y) {
  char buf[32];
  WRITE_LITERAL("\033[");
  WRITE_INT(y + 1);
  WRITE_LITERAL(";");
  WRITE_INT(x + 1);
  WRITE_LITERAL("H");
}

void termbox_impl::write_sgr(uint16_t fg, uint16_t bg) {
  char buf[32];

  if (fg == TB_DEFAULT && bg == TB_DEFAULT)
    return;

  switch (_outputmode) {
  case output_mode::mode256:
  case output_mode::mode216:
  case output_mode::grayscale:
    WRITE_LITERAL("\033[");
    if (fg != TB_DEFAULT) {
      WRITE_LITERAL("38;5;");
      WRITE_INT(fg);
      if (bg != TB_DEFAULT) {
        WRITE_LITERAL(";");
      }
    }
    if (bg != TB_DEFAULT) {
      WRITE_LITERAL("48;5;");
      WRITE_INT(bg);
    }
    WRITE_LITERAL("m");
    break;
  case output_mode::normal:
  default:
    WRITE_LITERAL("\033[");
    if (fg != TB_DEFAULT) {
      WRITE_LITERAL("3");
      WRITE_INT(fg - 1);
      if (bg != TB_DEFAULT) {
        WRITE_LITERAL(";");
      }
    }
    if (bg != TB_DEFAULT) {
      WRITE_LITERAL("4");
      WRITE_INT(bg - 1);
    }
    WRITE_LITERAL("m");
    break;
  }
}
void termbox_impl::send_attr(uint16_t fg, uint16_t bg) {
#define LAST_ATTR_INIT 0xFFFF
  static uint16_t lastfg = LAST_ATTR_INIT, lastbg = LAST_ATTR_INIT;
  if (fg != lastfg || bg != lastbg) {
    bytebuffer_puts(&_output_buffer, funcs[T_SGR0]);

    uint16_t fgcol;
    uint16_t bgcol;

    switch (_outputmode) {
    case output_mode::mode256:
      fgcol = fg & 0xFF;
      bgcol = bg & 0xFF;
      break;

    case output_mode::mode216:
      fgcol = fg & 0xFF;
      if (fgcol > 215)
        fgcol = 7;
      bgcol = bg & 0xFF;
      if (bgcol > 215)
        bgcol = 0;
      fgcol += 0x10;
      bgcol += 0x10;
      break;

    case output_mode::grayscale:
      fgcol = fg & 0xFF;
      if (fgcol > 23)
        fgcol = 23;
      bgcol = bg & 0xFF;
      if (bgcol > 23)
        bgcol = 0;
      fgcol += 0xe8;
      bgcol += 0xe8;
      break;

    case output_mode::normal:
    default:
      fgcol = fg & 0x0F;
      bgcol = bg & 0x0F;
    }

    if (fg & TB_BOLD)
      bytebuffer_puts(&_output_buffer, funcs[T_BOLD]);
    if (bg & TB_BOLD)
      bytebuffer_puts(&_output_buffer, funcs[T_BLINK]);
    if (fg & TB_UNDERLINE)
      bytebuffer_puts(&_output_buffer, funcs[T_UNDERLINE]);
    if ((fg & TB_REVERSE) || (bg & TB_REVERSE))
      bytebuffer_puts(&_output_buffer, funcs[T_REVERSE]);

    write_sgr(fgcol, bgcol);

    lastfg = fg;
    lastbg = bg;
  }
}

void termbox_impl::send_char(int x, int y, uint32_t c) {
  char buf[7];
  int bw = tb_utf8_unicode_to_char(buf, c);
  if (x - 1 != lastx || y != lasty)
    write_cursor(x, y);
  lastx = x;
  lasty = y;
  if (!c)
    buf[0] = ' '; // replace 0 with whitespace
  bytebuffer_append(&_output_buffer, buf, bw);
}

void termbox_impl::send_clear(void) {
  send_attr(foreground, background);
  bytebuffer_puts(&_output_buffer, funcs[T_CLEAR_SCREEN]);
  if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y))
    write_cursor(cursor_x, cursor_y);
  bytebuffer_flush(&_output_buffer, inout);

  /* we need to invalidate cursor position too and these two vars are
   * used only for simple cursor positioning optimization, cursor
   * actually may be in the correct place, but we simply discard
   * optimization once and it gives us simple solution for the case when
   * cursor moved */
  lastx = LAST_COORD_INIT;
  lasty = LAST_COORD_INIT;
}

int termbox_impl::read_up_to(int n) {
  assert(n > 0);
  const int prevlen = _input_buffer.len;
  bytebuffer_resize(&_input_buffer, prevlen + n);

  int read_n = 0;
  while (read_n <= n) {
    ssize_t r = 0;
    if (read_n < n) {
      r = read(inout, _input_buffer.buf + prevlen + read_n, n - read_n);
    }
#ifdef __CYGWIN__
    // While linux man for tty says when VMIN == 0 && VTIME == 0, read
    // should return 0 when there is nothing to read, cygwin's read returns
    // -1. Not sure why and if it's correct to ignore it, but let's pretend
    // it's zero.
    if (r < 0)
      r = 0;
#endif
    if (r < 0) {
      // EAGAIN / EWOULDBLOCK shouldn't occur here
      assert(errno != EAGAIN && errno != EWOULDBLOCK);
      return -1;
    } else if (r > 0) {
      read_n += r;
    } else {
      bytebuffer_resize(&_input_buffer, prevlen + read_n);
      return read_n;
    }
  }
  assert(!"unreachable");
  return 0;
}

termbox11::termbox11() : termbox11("/dev/tty") {}

termbox11::termbox11(std::string name)
    : termbox11(open(name.c_str(), O_RDWR)) {}

termbox11::termbox11(int fd) : _impl(std::make_unique<termbox_impl>()) {
  inout = fd;
  if (inout == -1) {
    throw std::runtime_error("failed to open tty");
  }

  if (init_term() < 0) {
    close(inout);
    throw std::runtime_error("unsupported terminal");
  }

  if (pipe(winch_fds) < 0) {
    close(inout);
    throw std::runtime_error("epipe trap");
  }

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigwinch_handler;
  sa.sa_flags = 0;
  sigaction(SIGWINCH, &sa, 0);

  tcgetattr(inout, &orig_tios);

  struct termios tios;
  memcpy(&tios, &orig_tios, sizeof(tios));

  tios.c_iflag &=
      ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
  tios.c_oflag &= ~OPOST;
  tios.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
  tios.c_cflag &= ~(CSIZE | PARENB);
  tios.c_cflag |= CS8;
  tios.c_cc[VMIN] = 0;
  tios.c_cc[VTIME] = 0;
  tcsetattr(inout, TCSAFLUSH, &tios);

  bytebuffer_init(&_impl->_input_buffer, 128);
  bytebuffer_init(&_impl->_output_buffer, 32 * 1024);

  bytebuffer_puts(&_impl->_output_buffer, funcs[T_ENTER_CA]);
  bytebuffer_puts(&_impl->_output_buffer, funcs[T_ENTER_KEYPAD]);
  bytebuffer_puts(&_impl->_output_buffer, funcs[T_HIDE_CURSOR]);
  _impl->send_clear();

  _impl->update_term_size();
  cellbuf_init(&back_buffer, _impl->_w, _impl->_h);
  cellbuf_init(&front_buffer, _impl->_w, _impl->_h);
  cellbuf_clear(&back_buffer);
  cellbuf_clear(&front_buffer);
}

termbox11::~termbox11() {
  bytebuffer_puts(&_impl->_output_buffer, funcs[T_SHOW_CURSOR]);
  bytebuffer_puts(&_impl->_output_buffer, funcs[T_SGR0]);
  bytebuffer_puts(&_impl->_output_buffer, funcs[T_CLEAR_SCREEN]);
  bytebuffer_puts(&_impl->_output_buffer, funcs[T_EXIT_CA]);
  bytebuffer_puts(&_impl->_output_buffer, funcs[T_EXIT_KEYPAD]);
  bytebuffer_puts(&_impl->_output_buffer, funcs[T_EXIT_MOUSE]);
  bytebuffer_flush(&_impl->_output_buffer, inout);
  tcsetattr(inout, TCSAFLUSH, &orig_tios);

  shutdown_term();
  close(inout);
  close(winch_fds[0]);
  close(winch_fds[1]);

  cellbuf_free(&back_buffer);
  cellbuf_free(&front_buffer);
  bytebuffer_free(&_impl->_output_buffer);
  bytebuffer_free(&_impl->_input_buffer);
  _impl->_w = _impl->_h = SIZE_MAX;
}

size_t termbox11::width() const { return _impl->_w; }
size_t termbox11::height() const { return _impl->_h; }

void termbox11::clear() {
  if (_impl->_buffer_size_change_request) {
    _impl->update_size();
    _impl->_buffer_size_change_request = 0;
  }
  cellbuf_clear(&back_buffer);
}

void termbox11::present() {
  int x, y, w, i;
  struct tb_cell *back, *front;

  /* invalidate cursor position */
  lastx = LAST_COORD_INIT;
  lasty = LAST_COORD_INIT;

  if (_impl->_buffer_size_change_request) {
    _impl->update_size();
    _impl->_buffer_size_change_request = false;
  }

  for (y = 0; y < front_buffer.height; ++y) {
    for (x = 0; x < front_buffer.width;) {
      back = &CELL(&back_buffer, x, y);
      front = &CELL(&front_buffer, x, y);
      w = wcwidth(back->ch);
      if (w < 1)
        w = 1;
      if (memcmp(back, front, sizeof(struct tb_cell)) == 0) {
        x += w;
        continue;
      }
      memcpy(front, back, sizeof(struct tb_cell));
      _impl->send_attr(back->fg, back->bg);
      if (w > 1 && x >= front_buffer.width - (w - 1)) {
        // Not enough room for wide ch, so send spaces
        for (i = x; i < front_buffer.width; ++i) {
          _impl->send_char(i, y, ' ');
        }
      } else {
        _impl->send_char(x, y, back->ch);
        for (i = 1; i < w; ++i) {
          front = &CELL(&front_buffer, x + i, y);
          front->ch = 0;
          front->fg = back->fg;
          front->bg = back->bg;
        }
      }
      x += w;
    }
  }
  if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y))
    _impl->write_cursor(cursor_x, cursor_y);
  bytebuffer_flush(&_impl->_output_buffer, inout);
}
event_type termbox11::poll_event(struct tb_event *event) {
  return _impl->wait_fill_event(event, 0);
}

event_type termbox11::peek_event(struct tb_event *event, int timeout) {
  struct timeval tv;
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout - (tv.tv_sec * 1000)) * 1000;
  return _impl->wait_fill_event(event, &tv);
}

void termbox11::set_cursor(int cx, int cy) {
  if (IS_CURSOR_HIDDEN(cursor_x, cursor_y) && !IS_CURSOR_HIDDEN(cx, cy))
    bytebuffer_puts(&_impl->_output_buffer, funcs[T_SHOW_CURSOR]);

  if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y) && IS_CURSOR_HIDDEN(cx, cy))
    bytebuffer_puts(&_impl->_output_buffer, funcs[T_HIDE_CURSOR]);

  cursor_x = cx;
  cursor_y = cy;
  if (!IS_CURSOR_HIDDEN(cursor_x, cursor_y))
    _impl->write_cursor(cursor_x, cursor_y);
}

void termbox11::select_input_mode(struct input_mode mode) {
  if (!mode.escaped && !mode.alt)
    mode.escaped = true;

  /* technically termbox can handle that, but let's be nice and show here
     what mode is actually used */
  if (mode.escaped && mode.alt)
    mode.alt = false;

  _impl->_inputmode = mode;
  if (mode.mouse) {
    bytebuffer_puts(&_impl->_output_buffer, funcs[T_ENTER_MOUSE]);
    bytebuffer_flush(&_impl->_output_buffer, inout);
  } else {
    bytebuffer_puts(&_impl->_output_buffer, funcs[T_EXIT_MOUSE]);
    bytebuffer_flush(&_impl->_output_buffer, inout);
  }
}
input_mode termbox11::input_mode() { return _impl->_inputmode; }

void termbox11::select_output_mode(enum output_mode mode) {
  _impl->_outputmode = mode;
}

output_mode termbox11::output_mode() { return _impl->_outputmode; }
