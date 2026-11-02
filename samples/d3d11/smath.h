#pragma once

#include <xmmintrin.h>

namespace sm {

constexpr float pi = 3.14159265358979323846f;

inline float sqrt(float x) { return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x))); }
inline float abs(float x) { return x < 0.0f ? -x : x; }
inline float floor(float x)
{
    const float t = static_cast<float>(static_cast<long long>(x));
    return t > x ? t - 1.0f : t;
}
inline float fract(float x) { return x - floor(x); }

inline float sin(float x)
{
    const float two_pi = 2.0f * pi;
    x = x - two_pi * floor((x + pi) / two_pi);
    const float x2 = x * x;
    return x * (1.0f + x2 * (-1.6666667e-1f + x2 * (8.3333310e-3f + x2 * (-1.9840874e-4f +
                x2 * (2.7525562e-6f + x2 * (-2.3889859e-8f + x2 * 1.6405e-10f))))));
}
inline float cos(float x) { return sin(x + 0.5f * pi); }
inline float tan(float x) { return sin(x) / cos(x); }
inline float radians(float degrees) { return degrees * pi / 180.0f; }
inline float min(float a, float b) { return a < b ? a : b; }
inline float max(float a, float b) { return a > b ? a : b; }
inline float clamp(float x, float a, float b) { return min(max(x, a), b); }
inline float mix(float a, float b, float t) { return a + (b - a) * t; }

struct Random {
    unsigned state = 0x9E3779B9u;
    explicit Random(unsigned seed) : state(seed ? seed : 1u) {}
    unsigned next()
    {
        state ^= state << 13u;
        state ^= state >> 17u;
        state ^= state << 5u;
        return state;
    }
    float unit() { return static_cast<float>(next() & 0xFFFFFFu) / 16777216.0f; }
    float range(float a, float b) { return a + (b - a) * unit(); }
};

struct float2 { float x, y; };
struct float3 {
    float x, y, z;
    float3 operator+(float3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    float3 operator-(float3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    float3 operator*(float s) const { return {x * s, y * s, z * s}; }
    float3 operator-() const { return {-x, -y, -z}; }
};
struct float4 { float x, y, z, w; };

inline float dot(float3 a, float3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float3 cross(float3 a, float3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(float3 a) { return sqrt(dot(a, a)); }
inline float3 normalize(float3 a)
{
    const float l = length(a);
    return l > 0.0f ? a * (1.0f / l) : a;
}

struct float4x4 {
    float m[4][4];
    static float4x4 identity()
    {
        float4x4 r = {};
        for (int i = 0; i < 4; i++) r.m[i][i] = 1.0f;
        return r;
    }
    float4x4 operator*(const float4x4 &o) const
    {
        float4x4 r = {};
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                for (int k = 0; k < 4; k++) r.m[i][j] += m[i][k] * o.m[k][j];
        return r;
    }
};

inline float4x4 translate(float3 t)
{
    float4x4 r = float4x4::identity();
    r.m[3][0] = t.x;
    r.m[3][1] = t.y;
    r.m[3][2] = t.z;
    return r;
}
inline float4x4 scale(float3 s)
{
    float4x4 r = float4x4::identity();
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    r.m[2][2] = s.z;
    return r;
}
inline float4x4 rotate_x(float a)
{
    float4x4 r = float4x4::identity();
    r.m[1][1] = cos(a); r.m[1][2] = sin(a);
    r.m[2][1] = -sin(a); r.m[2][2] = cos(a);
    return r;
}
inline float4x4 rotate_y(float a)
{
    float4x4 r = float4x4::identity();
    r.m[0][0] = cos(a); r.m[0][2] = -sin(a);
    r.m[2][0] = sin(a); r.m[2][2] = cos(a);
    return r;
}
inline float4x4 rotate_z(float a)
{
    float4x4 r = float4x4::identity();
    r.m[0][0] = cos(a); r.m[0][1] = sin(a);
    r.m[1][0] = -sin(a); r.m[1][1] = cos(a);
    return r;
}
inline float4x4 perspective(float fov_y, float aspect, float z_near, float z_far)
{
    const float y = 1.0f / tan(fov_y * 0.5f);
    float4x4 r = {};
    r.m[0][0] = y / aspect;
    r.m[1][1] = y;
    r.m[2][2] = z_far / (z_far - z_near);
    r.m[2][3] = 1.0f;
    r.m[3][2] = -z_near * z_far / (z_far - z_near);
    return r;
}
inline float4x4 orthographic(float width, float height, float z_near, float z_far)
{
    float4x4 r = float4x4::identity();
    r.m[0][0] = 2.0f / width;
    r.m[1][1] = 2.0f / height;
    r.m[2][2] = 1.0f / (z_far - z_near);
    r.m[3][2] = -z_near / (z_far - z_near);
    return r;
}
inline float4x4 look_at(float3 eye, float3 at, float3 up)
{
    const float3 z = normalize(at - eye);
    const float3 x = normalize(cross(up, z));
    const float3 y = cross(z, x);
    float4x4 r = float4x4::identity();
    r.m[0][0] = x.x; r.m[1][0] = x.y; r.m[2][0] = x.z;
    r.m[0][1] = y.x; r.m[1][1] = y.y; r.m[2][1] = y.z;
    r.m[0][2] = z.x; r.m[1][2] = z.y; r.m[2][2] = z.z;
    r.m[3][0] = -dot(x, eye);
    r.m[3][1] = -dot(y, eye);
    r.m[3][2] = -dot(z, eye);
    return r;
}

}
