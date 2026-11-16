#pragma once
// Generic digital coordinate mapping; not a physical movement controller.
#include "anaiero/contracts/cell_behavior.hpp"
namespace anaiero::cells {
class BoundedPositionPort final : public ISpatialPort {
public:
  bool translate(const Vec3& start,const Vec3& desired,Vec3& result) noexcept override {
    (void)start;
    if(desired.x<0||desired.x>255||desired.y<0||desired.y>255||
       desired.z<0||desired.z>255)return false;
    result=desired;return true;
  }
};
} // namespace anaiero::cells
