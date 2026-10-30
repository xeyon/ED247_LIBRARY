/* -*- mode: c++; c-basic-offset: 2 -*-  */
#include "ed247.h"

#ifndef _MSC_VER
# include <time.h>
# include <sys/time.h>
// If system do not support CLOCK_MONOTONIC_RAW, fallback to CLOCK_MONOTONIC
# ifndef CLOCK_MONOTONIC_RAW
#  define CLOCK_MONOTONIC_RAW CLOCK_MONOTONIC
# endif
#endif
#ifdef _WIN32
# include <windows.h>
#endif


// ed247_time.h implementation
namespace ed247 {
  uint64_t get_monotonic_time_us()
  {
#ifdef __unix__
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
    return ((uint64_t)tp.tv_sec) * 1000000LL + ((uint64_t)tp.tv_nsec) / 1000LL;
#else
    LARGE_INTEGER s_frequency;
    BOOL s_use_qpc = QueryPerformanceFrequency(&s_frequency);
    if (s_use_qpc) {
      LARGE_INTEGER now;
      QueryPerformanceCounter(&now);
      return (1000000LL * now.QuadPart) / s_frequency.QuadPart;
    } else {
      return GetTickCount() * 1000LL;
    }
#endif
  }

  void sleep_us(uint32_t duration_us)
  {
#ifdef _WIN32
    Sleep(duration_us / 1000);
#else
    struct timespec ts;
    ts.tv_sec = duration_us / (1000 * 1000);
    ts.tv_nsec = (duration_us % (1000 * 1000)) * 1000;
    nanosleep(&ts, NULL);
#endif
  }
}


// ed247.h time implementation
namespace ed247 {
  static ed247_get_time_t get_transport_timestamp = &ed247_get_time;
  static ed247_get_time_t get_receive_timestamp = &ed247_get_time;
}

void ed247_get_time(ed247_timestamp_t* timestamp)
{
#ifdef _WIN32
  static const uint64_t WINDOWS_TO_UNIX_EPOCH_100NS = 116444736000000000ULL;
  FILETIME file_time;
  GetSystemTimeAsFileTime(&file_time);
  const uint64_t time_100ns = (static_cast<uint64_t>(file_time.dwHighDateTime) << 32)
                            | file_time.dwLowDateTime;
  const uint64_t unix_time_100ns = time_100ns - WINDOWS_TO_UNIX_EPOCH_100NS;
  timestamp->epoch_s = static_cast<uint32_t>(unix_time_100ns / 10000000ULL);
  timestamp->offset_ns = static_cast<uint32_t>((unix_time_100ns % 10000000ULL) * 100ULL);
#else
  struct timeval tv;
  gettimeofday(&tv, NULL);
  timestamp->epoch_s = (uint32_t)tv.tv_sec;
  timestamp->offset_ns = (uint32_t)tv.tv_usec*1000LL;
#endif
}

void ed247_set_transport_timestamp_callback(ed247_get_time_t callback)
{
  ed247::get_transport_timestamp = callback;
}

void ed247_set_receive_timestamp_callback(ed247_get_time_t callback)
{
  ed247::get_receive_timestamp = callback;
}

void ed247_get_transport_timestamp(ed247_timestamp_t* timestamp)
{
  (*ed247::get_transport_timestamp)(timestamp);
}

void ed247_get_receive_timestamp(ed247_timestamp_t* timestamp)
{
  (*ed247::get_receive_timestamp)(timestamp);
}
