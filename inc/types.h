#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

namespace farsight {

  constexpr static float FLOOR_BASE_Y = -0.25;

  struct Point3f
  {
    using CoordType = float;

    union
    {
      struct
      {
        CoordType x, y, z;
      };

      CoordType arr[3];
    };

    Point3f() = default;

    Point3f(glm::vec3 v)
      : x(v.x)
      , y(v.y)
      , z(v.z)
    {}

    Point3f(CoordType x, CoordType y, CoordType z)
      : x(x)
      , y(y)
      , z(z)
    {}

    operator glm::vec3() const { return glm::vec3{ x, y, z }; }

    Point3f &
    operator+=(const glm::vec3 &v)
    {
      this->x += v.x;
      this->y += v.y;
      this->z += v.z;

      return *this;
    }
  };

  union ColorType
  {
    struct
    {
      uint8_t r, g, b;
    };
    uint32_t packed;
  };

  constexpr ColorType RED =
    ColorType{ { .r = 0xff, .g = 0x00, .b = 0x00 } };
  constexpr ColorType GREEN =
    ColorType{ { .r = 0x00, .g = 0xff, .b = 0x00 } };
  constexpr ColorType BLUE =
    ColorType{ { .r = 0x00, .g = 0x00, .b = 0xff } };
  constexpr ColorType WHITE =
    ColorType{ { .r = 0xff, .g = 0xff, .b = 0xff } };

  struct Point3fc : public Point3f
  {
    ColorType color = WHITE;

    Point3fc() = default;

    Point3fc(glm::vec3 v, ColorType color)
      : Point3f(v)
      , color(color)
    {}

    Point3fc(float x, float y, float z, ColorType color)
      : Point3f(x, y, z)
      , color(color)
    {}

    Point3fc(Point3f p3)
      : Point3f(p3)
      , color(WHITE)
    {}

    Point3fc(std::initializer_list<CoordType> il)
    {
      auto size = std::min(std::size(this->arr), std::size(il));
      std::copy(std::begin(il), std::begin(il) + size, this->arr);
    }

    operator glm::vec3() const { return glm::vec3{ x, y, z }; }

    Point3fc &
    operator+=(const glm::vec3 &v)
    {
      this->x += v.x;
      this->y += v.y;
      this->z += v.z;

      return *this;
    }

    Point3fc &
    operator=(const glm::vec3 &v)
    {
      this->x = v.x;
      this->y = v.y;
      this->z = v.z;

      return *this;
    }
  };

  struct Point2i
  {
    int x, y;

    Point2i() = default;

    Point2i(glm::vec3 v)
      : x(v.x)
      , y(v.y)
    {}

    Point2i(int x, int y)
      : x(x)
      , y(y)
    {}

    operator glm::vec2() const { return glm::vec2{ x, y }; }
  };

  struct Rectfc
  {
    std::array<Point3fc, 4> verts;
    ColorType color = WHITE;
  };

  using PointArraySimple = std::vector<Point3f>;
  using RectArraySimple = std::vector<Rectfc>;
  using PointArray = std::vector<Point3fc>;
  using RectArray = std::vector<Rectfc>;

  struct CameraShot
  {
    size_t width = 1;
    PointArray points{ { 0, 0, 0, WHITE } };
    glm::vec3 tvec{ 0.0f, 0.0f, 0.0f };
    glm::vec3 rvec{ 0.0f, 0.0f, 0.0f };
    float floor_level = 0.0f;
  };

  struct Context3D
  {
  public:
    using PointInfoLocked =
      std::tuple<std::unique_lock<std::mutex>, CameraShot &>;
    using PointInfo = PointArray;

    using MarkInfoLocked =
      std::tuple<std::unique_lock<std::mutex>, RectArray &>;
    using MarkInfo = RectArray;

    void
    update_cam1(PointArray &&points, size_t width)
    {
      std::unique_lock lck{ this->mtx };

      this->camshot1.points = std::move(points);
      this->camshot1.width = width;
    }

    void
    update_cam2(PointArray &&points, size_t width)
    {
      std::unique_lock lck{ this->mtx };

      this->camshot2.points = std::move(points);
      this->camshot2.width = width;
    }

    PointInfoLocked
    get_points_cam1()
    {
      return { std::unique_lock(this->mtx), this->camshot1 };
    }

    PointInfoLocked
    get_points_cam2()
    {
      return { std::unique_lock(this->mtx), this->camshot2 };
    }

    MarkInfoLocked
    get_marks()
    {
      return { std::unique_lock(this->mtx), this->marks };
    }

    PointInfo
    get_translated_points(const CameraShot &cam)
    {
      std::unique_lock lck{ this->mtx };
      auto ret = cam.points;
      lck.unlock();

      glm::vec3 tvec = cam.tvec;
      glm::vec3 rvec = cam.rvec;

      for (auto &point : ret)
      {
        point.x += tvec.x;
        point.y += tvec.y;
        point.z += tvec.z;

        point = glm::rotateX(static_cast<glm::vec3>(point), rvec.x);
        point = glm::rotateY(static_cast<glm::vec3>(point), rvec.y);
        point = glm::rotateZ(static_cast<glm::vec3>(point), rvec.z);

        if (point.y <= (FLOOR_BASE_Y + cam.floor_level))
        {
          point.x = NAN;
          point.y = NAN;
          point.z = NAN;
        }
      }

      return ret;
    }

    PointInfo
    get_translated_points_cam1()
    {
      return this->get_translated_points(this->camshot1);
    }

    PointInfo
    get_translated_points_cam2()
    {
      return this->get_translated_points(this->camshot2);
    }

    glm::vec3
    get_tvec_cam1()
    {
      std::unique_lock lck{ this->mtx };
      return this->camshot1.tvec;
    }

    glm::vec3
    get_rvec_cam1()
    {
      std::unique_lock lck{ this->mtx };
      return this->camshot1.rvec;
    }

    glm::vec3
    get_tvec_cam2()
    {
      std::unique_lock lck{ this->mtx };
      return this->camshot2.tvec;
    }

    glm::vec3
    get_rvec_cam2()
    {
      std::unique_lock lck{ this->mtx };
      return this->camshot2.rvec;
    }

    void
    set_tvec_cam1(glm::vec3 v)
    {
      std::unique_lock lck{ this->mtx };
      this->camshot1.tvec = v;
    }

    void
    set_rvec_cam1(glm::vec3 v)
    {
      std::unique_lock lck{ this->mtx };
      this->camshot1.rvec = v;
    }

    void
    set_tvec_cam2(glm::vec3 v)
    {
      std::unique_lock lck{ this->mtx };
      this->camshot2.tvec = v;
    }

    void
    set_rvec_cam2(glm::vec3 v)
    {
      std::unique_lock lck{ this->mtx };
      this->camshot2.rvec = v;
    }

    void
    set_floor_level(float level)
    {
      std::unique_lock lck{ this->mtx };
      this->camshot1.floor_level = level;
      this->camshot2.floor_level = level;
    }

    float
    get_floor_level() const
    {
      std::unique_lock lck{ this->mtx };
      assert(this->camshot1.floor_level == this->camshot2.floor_level);

      return this->camshot1.floor_level;
    }

    void
    mark(Rectfc rect, glm::vec3 tvec, glm::vec3 rvec)
    {
      std::unique_lock lck{ this->mtx };

      for (auto &v : rect.verts) {
        v += tvec;

        v = glm::rotateX(static_cast<glm::vec3>(v), rvec.x);
        v = glm::rotateY(static_cast<glm::vec3>(v), rvec.y);
        v = glm::rotateZ(static_cast<glm::vec3>(v), rvec.z);
      }

      marks.emplace_back(std::move(rect));
    }

    void
    reset_marks()
    {
      std::unique_lock lck{ this->mtx };
      marks.clear();
    }

  private:
    mutable std::mutex mtx;
    CameraShot camshot1, camshot2;
    RectArray marks;
  };

} // namespace farsight
