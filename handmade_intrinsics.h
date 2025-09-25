#if !defined(HANDMADE_INTRINSICS_H)

#include<math.h> // TODO - eventually remove 

inline i32 f32_round_to_i32(f32 val) {
  i32 result = (i32)(val + 0.5f);
  return result;
}

inline u32 f32_round_to_u32(f32 val) {
  u32 result = (u32)(val + 0.5f);
  return result;
}

inline i32 f32_truncate_to_i32(f32 val) {
  i32 result = (i32)val;
  return result;
}

inline i32 f32_floor_to_i32(f32 val) {
  i32 result = (int)floorf(val);
  return result;
}

inline f32 sin(f32 angle) {
  f32 result = sinf(angle);
  return result;
}

inline f32 cos(f32 angle) {
  f32 result = cosf(angle);
  return result;
}

inline f32 atan2(f32 y, f32 x) {
  f32 result = atan2f(y, x);
  return result;
}


#define HANDMADE_INTRINSICS_H
#endif
