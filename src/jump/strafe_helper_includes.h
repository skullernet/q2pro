#ifndef STRAFE_HELPER_INCLUDES_H
#define STRAFE_HELPER_INCLUDES_H
#define STRAFE_HELPER_CUSTOMIZATION_DISABLE_DRAW_SPEED // use the "draw cl_ups"
                                                       // command  instead

#include "inc/shared/shared.h"
#include "refresh/refresh.h"
#include "shared/shared.h"
#include "src/jump/strafe_helper/strafe_helper_customization.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static inline uint32_t getColor(int element_id) {
  switch (element_id) {
  case shc_ElementId_AcceleratingAngles:
    return MakeColor(0, 128, 32, 96); // shi_color_accelerating
  case shc_ElementId_OptimalAngle:
    return MakeColor(0, 255, 64, 192); // shi_color_optimal
  case shc_ElementId_CenterMarker:
    return MakeColor(255, 255, 255, 192); // shi_color_center_marker
  default:
    return MakeColor(255, 0, 0, 255); // Default color (unused?)
  }
}

void shc_drawFilledRectangle(const float x, const float y, const float w,
                             const float h, enum shc_ElementId element_id) {

  R_DrawFill32(roundf(x), roundf(y), roundf(w), roundf(h),
               getColor(element_id));
}

#endif // STRAFE_HELPER_INCLUDES_H
