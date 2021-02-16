//
// Created by sylvestre on 16/02/2021.
//

#ifndef TERMBOX11_BYTEBUFFER_H
#define TERMBOX11_BYTEBUFFER_H

#include <cstdint>

class bytebuffer {
public:
  void resize(std::size_t);
  void flush(int fd);
  void truncate(std::size_t n);
};

#endif // TERMBOX11_BYTEBUFFER_H
