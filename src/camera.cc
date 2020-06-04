#include <cassert>

#include "camera.h"

#include <fmt/format.h>
namespace farsight {

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-designator"
#endif

  // clang-format off
  const glm::vec3 faces_position[6] {
    [0] = {-0.25f, 0.00f, 0.25f },
    [1] = { 0.00f, 0.00f, 0.00f },
    [2] = { 0.25f, 0.00f, 0.25f },
    [3] = { 0.00f,-0.25f, 0.25f },
    [4] = { 0.00f, 0.25f, 0.25f },
    [5] = { 0.00f, 0.00f, 0.50f },
  };
  // clang-format on

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

  const glm::vec3 front_face = faces_position[1];

  constexpr static void
  check_face_id(int id)
  {
    // 0 is bottom face and cannot be seen
    assert(id < std::size(faces_position) && id >= 0 && id != 3);
  }

  static glm::vec3
  calculate_face_offset(int id)
  {
    check_face_id(id);
    return faces_position[id] * -1.0f;
  }

  void
  camera2real(std::vector<Point3f> &points,
              glm::vec3 tvec,
              glm::mat3x3 rot,
              int id)
  {
    check_face_id(id);

    // Get marker offset from camera
    glm::vec3 camera_pos = tvec;

    // Apply relative face offset 
    camera_pos += calculate_face_offset(id);

    // Get camera position relative to front marker (world 0,0,0)
    camera_pos *= -1.0f;


    fmt::print("Camera pos: {} {} {}", camera_pos.x, camera_pos.y, camera_pos.z);

    // Apply camera offset and rotation to all points.
    for (auto &point : points) {
      point.x += camera_pos.x;
      point.y += camera_pos.y;
      point.z += camera_pos.z;

     // point = rot * point;
    }
  }

} // namespace farsight
