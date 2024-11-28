#ifndef STRAFE_HELPER_CUSTOMIZATION_H
#define STRAFE_HELPER_CUSTOMIZATION_H

#ifdef __cplusplus
extern "C" {
#endif

enum shc_ElementId {
    shc_ElementId_AcceleratingAngles,
    shc_ElementId_OptimalAngle,
    shc_ElementId_CenterMarker,
    shc_ElementId_SpeedText,
};

void shc_drawFilledRectangle(float x, float y, float w, float h,
                             enum shc_ElementId element_id);
void shc_drawString(float x, float y, const char* string, float scale,
                    enum shc_ElementId element_id);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STRAFE_HELPER_CUSTOMIZATION_H */
