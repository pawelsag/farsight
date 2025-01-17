#pragma once

#include <cstdint>
#include <vector>

#include "types.h"

extern "C" {
void glutPostRedisplay();
}

namespace farsight {

  extern Context3D context3D;

  void
  init3d();

  inline void
  update_points_cam1(PointArraySimple points, size_t width)
  {
    context3D.update_cam1(PointArray(std::begin(points), std::end(points)), width);
  }

  inline void
  update_points_cam1(PointArray points, size_t width)
  {
    context3D.update_cam1(std::move(points), width);
  }

  inline void
  update_points_cam2(PointArraySimple points, size_t width)
  {
    context3D.update_cam1(PointArray(std::begin(points), std::end(points)), width);
  }

  inline void
  update_points_cam2(PointArray points, size_t width)
  {
    context3D.update_cam2(std::move(points), width);
  }

  inline glm::vec3
  get_tvec_cam1()
  {
    return context3D.get_tvec_cam1();
  }

  inline glm::vec3
  get_rvec_cam1()
  {
    return context3D.get_rvec_cam1();
  }

  inline glm::vec3
  get_tvec_cam2()
  {
    return context3D.get_tvec_cam2();
  }

  inline glm::vec3
  get_rvec_cam2()
  {
    return context3D.get_rvec_cam2();
  }

  inline void
  set_tvec_cam1(glm::vec3 v)
  {
    context3D.set_tvec_cam1(v);
  }

  inline void
  set_rvec_cam1(glm::vec3 v)
  {
    context3D.set_rvec_cam1(v);
  }

  inline void
  set_tvec_cam2(glm::vec3 v)
  {
    context3D.set_tvec_cam2(v);
  }

  inline void
  set_rvec_cam2(glm::vec3 v)
  {
    context3D.set_rvec_cam2(v);
  }

  inline Context3D::PointInfo
  get_translated_points_cam1()
  {
    return context3D.get_translated_points_cam1();
  }

  inline Context3D::PointInfo
  get_translated_points_cam2()
  {
    return context3D.get_translated_points_cam2();
  }

  inline void
  set_floor_level(float level)
  {
    context3D.set_floor_level(level);
  }

  inline float
  get_floor_level()
  {
    return context3D.get_floor_level();
  }

  inline void
  add_marker(Rectfc rect, glm::vec3 tvec = {0.0f, 0.0f, 0.0f}, glm::vec3 rvec = {0.0f, 0.0f, 0.0f})
  {
    context3D.mark(rect, tvec, rvec);
  }

  inline void
  reset_marks()
  {
    context3D.reset_marks();
  }

} // namespace farsight
