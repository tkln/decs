#ifndef VEC3_H
#define VEC3_H

struct vec3 {
    union {
        float e[3];
        struct {
            float x, y, z;
        };
    };
};

static inline struct vec3 vec3_add(const struct vec3 a, const struct vec3 b)
{
    struct vec3 s = { a.x + b.x, a.y + b.y, a.z + b.z };
    return s;
}

static inline struct vec3 vec3_muls(const struct vec3 a, const float b)
{
    struct vec3 r = { a.x * b, a.y * b, a.z * b };
    return r;
}

static inline struct vec3 vec3_pow2(const struct vec3 a)
{
    struct vec3 r = { a.x * a.x, a.y * a.y, a.z * a.z };
    return r;
}

#endif
