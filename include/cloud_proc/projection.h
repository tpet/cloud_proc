#pragma once

#include <cmath>
#include <sensor_msgs/point_cloud2_iterator.h>
#include <sensor_msgs/PointCloud2.h>

namespace cloud_proc
{

template<class T>
bool is_point_valid(T x, T y, T z)
{
  return std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
      && (x != 0 || y != 0 || z != 0);
}

template<class T>
T azimuth(T x, T y, T z)
{
  return std::atan2(y, x);
}

template<class T>
T elevation(T x, T y, T z)
{
  return std::atan2(z, std::hypot(x, y));
}

template<class T>
class Projection
{
public:
  enum Keep
  {
      KEEP_FIRST = 0,
      KEEP_LAST = 1,
      KEEP_CLOSEST = 2,
      KEEP_FARTHEST = 3,
  };

  int height_ = 0;
  int width_ = 0;
  T f_azimuth_ = std::numeric_limits<T>::quiet_NaN();
  T f_elevation_ = std::numeric_limits<T>::quiet_NaN();
  T c_azimuth_ = std::numeric_limits<T>::quiet_NaN();
  T c_elevation_ = std::numeric_limits<T>::quiet_NaN();
  int keep_ = KEEP_LAST;
  bool azimuth_only_ = false;

  bool process(const sensor_msgs::PointCloud2& input, sensor_msgs::PointCloud2& output)
  {
    assert(input.height >= 1);
    assert(input.width >= 1);
    assert(input.row_step == input.width * input.point_step);
    output.header = input.header;
    output.height = (azimuth_only_ || height_ == 0) ? input.height : height_;
    assert(output.height >= 1);
    output.width = (width_ == 0) ? input.width : width_;
    assert(output.width >= 1);
    output.fields = input.fields;
    output.is_bigendian = input.is_bigendian;
    output.point_step = input.point_step;
    output.row_step = output.width * output.point_step;
    output.data.clear();
    // Initialize output points to invalid (zero float are zero bytes).
    output.data.resize(output.height * output.width * output.point_step, 0);
    output.is_dense = input.is_dense;

    T f_azimuth = (std::isfinite(f_azimuth_) && f_azimuth_ != 0) ? f_azimuth_ : -T(output.width) / (2 * M_PI);
    T f_elevation = (std::isfinite(f_elevation_) && f_elevation_ != 0) ? f_elevation_ : -T(output.height) / (M_PI / 2);
    T c_azimuth = std::isfinite(c_azimuth_) ? c_azimuth_ : T(output.width) / 2 - 0.5;
    T c_elevation = std::isfinite(c_elevation_) ? c_elevation_ : T(output.height) / 2 - 0.5;

    sensor_msgs::PointCloud2ConstIterator<T> x_in(input, "x");
    sensor_msgs::PointCloud2Iterator<T> x_out_begin(output, "x");
    for (size_t i_in = 0; i_in < input.height; ++i_in)
    {
      for (size_t j_in = 0; j_in < input.width; ++j_in, ++x_in)
      {
        if (!is_point_valid(x_in[0], x_in[1], x_in[2]))
          continue;
        T u, v;
        u = f_azimuth * azimuth(x_in[0], x_in[1], x_in[2]) + c_azimuth;
        // Move [-0.5, 0.5) to [0, 1).
        u += 0.5;
        // Skip points outside the image.
        if (u < 0 || u >= output.width || std::isnan(u))
          continue;
        size_t j_out = u;
        size_t i_out;
        if (azimuth_only_)
        {
          i_out = i_in;
        }
        else
        {
          v = f_elevation * elevation(x_in[0], x_in[1], x_in[2]) + c_elevation;
          v += 0.5;
          if (v < 0 || v >= output.height || std::isnan(v))
            continue;
          i_out = v;
        }
        // Skip if target pixel already contains a valid point.
        if (keep_ == KEEP_FIRST)
        {
          auto x_out = x_out_begin + i_out * output.width + j_out;
          if (is_point_valid(x_out[0], x_out[1], x_out[2]))
            continue;
        }
        // Skip if target pixel contains a closer point.
        else if (keep_ == KEEP_CLOSEST)
        {
          auto x_out = x_out_begin + i_out * output.width + j_out;
          if (is_point_valid(x_out[0], x_out[1], x_out[2])
  //            && std::hypot(x_out[0], x_out[1], x_out[2]) < std::hypot(x_in[0], x_in[1], x_in[2]))
              && std::hypot(std::hypot(x_out[0], x_out[1]), x_out[2]) <= std::hypot(std::hypot(x_in[0], x_in[1]), x_in[2]))
            continue;
        }
        if (keep_ == KEEP_FARTHEST)
        {
          auto x_out = x_out_begin + i_out * output.width + j_out;
          if (is_point_valid(x_out[0], x_out[1], x_out[2])
  //            && std::hypot(x_out[0], x_out[1], x_out[2]) < std::hypot(x_in[0], x_in[1], x_in[2]))
              && std::hypot(std::hypot(x_out[0], x_out[1]), x_out[2]) >= std::hypot(std::hypot(x_in[0], x_in[1]), x_in[2]))
            continue;
        }

        // Copy the whole point to target pixel.
        std::copy(input.data.begin() + i_in * input.row_step + j_in * input.point_step,
                  input.data.begin() + i_in * input.row_step + (j_in + 1) * input.point_step,
                  output.data.begin() + i_out * output.row_step + j_out * output.point_step);
      }
    }
    return true;
  }

};

}
