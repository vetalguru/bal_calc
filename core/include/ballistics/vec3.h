#ifndef BALLISTICS_VEC3_H
#define BALLISTICS_VEC3_H

#include <cmath>

namespace ballistics {

// 3-D vector in the shooter's frame (SI units):
//   x - horizontal, towards the target azimuth (down-range)
//   y - vertical, up
//   z - horizontal, to the right of the line of fire
struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    constexpr Vec3& operator+=(const Vec3& o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    constexpr Vec3& operator-=(const Vec3& o) {
        x -= o.x;
        y -= o.y;
        z -= o.z;
        return *this;
    }
    constexpr Vec3& operator*=(double k) {
        x *= k;
        y *= k;
        z *= k;
        return *this;
    }

    double Norm() const { return std::sqrt(x * x + y * y + z * z); }
};

constexpr Vec3 operator+(Vec3 a, const Vec3& b) { return a += b; }
constexpr Vec3 operator-(Vec3 a, const Vec3& b) { return a -= b; }
constexpr Vec3 operator-(const Vec3& a) { return {-a.x, -a.y, -a.z}; }
constexpr Vec3 operator*(Vec3 a, double k) { return a *= k; }
constexpr Vec3 operator*(double k, Vec3 a) { return a *= k; }
constexpr double Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 Cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

}  // namespace ballistics

#endif  // BALLISTICS_VEC3_H
