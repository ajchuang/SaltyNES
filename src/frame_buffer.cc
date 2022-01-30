/*
  The file is added for supporting partial update. The key features
  1. tracking the updated area
  2. support extraction of the updated area(for update frame buffer)
 */
#include "SaltyNES.h"


frame_buffer* frame_buffer::get_frame_buffer() {
  static frame_buffer* singleton = nullptr;
  if (singleton)
    return singleton;
  singleton = new frame_buffer();
  return singleton;
}

/* the screen buf has been displayed. reset the last updated scan line */
void frame_buffer::rendered() {
  m_largest_updated_line = -1;
}

int frame_buffer::get_pixel(const int linear_order) {
  return m_buf[linear_order];
}

int frame_buffer::get_pixel(const int x, const int y) {
  const int offset = y * RES_WIDTH + x;
  return m_buf[offset];
}

void frame_buffer::set_pixel(const int linear_order, const int value) {
  const int pos_y = linear_order / RES_WIDTH;
  if (m_buf[pos_y] != value) {
    m_largest_updated_line = std::max(m_largest_updated_line, pos_y);
    m_buf[linear_order] = value;
  }
}

void frame_buffer::set_pixel(const int x, const int y, const int value) {
  const int offset = y * RES_WIDTH + x;
  if (m_buf[offset] != value) {
    m_largest_updated_line = std::max(m_largest_updated_line, y);
    m_buf[offset] = value;
  }
}

const void* frame_buffer::data_ptr() {
  return data_ptr(0, 0);
}

const void* frame_buffer::data_ptr(const int x, const int y) {
  const size_t offset = (y * RES_WIDTH + x);
  return (const void*)(m_buf.data() + offset);
}
  
frame_buffer::frame_buffer() :
    m_largest_updated_line(RES_HEIGHT) {
  std::fill(m_buf.begin(), m_buf.end(), 0);
}
