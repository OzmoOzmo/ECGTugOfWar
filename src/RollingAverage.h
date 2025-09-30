#pragma once
#include <Arduino.h>

template<typename T>
class RollingAverage {
public:
  RollingAverage(size_t capacity = 10) : _cap(capacity), _buf(nullptr), _idx(0), _count(0), _sum(0.0) {
    if (_cap == 0) _cap = 1;
    _buf = new T[_cap];
    clear();
  }
  ~RollingAverage() { delete[] _buf; }

  void add(T v) {
    if (_count < _cap) {
      _buf[_idx] = v;
      _sum += (double)v;
      _count++;
    } else {
      // overwrite oldest
      _sum -= (double)_buf[_idx];
      _buf[_idx] = v;
      _sum += (double)v;
    }
    _idx = (_idx + 1) % _cap;
  }

  double getAverage() const {
    if (_count == 0) return 0.0;
    return _sum / (double)_count;
  }

  void clear() {
    for (size_t i = 0; i < _cap; ++i) _buf[i] = (T)0;
    _idx = 0;
    _count = 0;
    _sum = 0.0;
  }

  size_t count() const { return _count; }
  size_t capacity() const { return _cap; }

private:
  size_t _cap;
  T* _buf;
  size_t _idx;
  size_t _count;
  double _sum;
};