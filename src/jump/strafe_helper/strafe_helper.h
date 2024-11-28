#ifndef STRAFE_HELPER_H
#define STRAFE_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

struct StrafeHelperParams {
    int center;
    int center_marker;
    float scale;
    float height;
    float y;
    float speed_scale;
    float speed_x;
    float speed_y;
};

void StrafeHelper_SetAccelerationValues(const float forward[3],
                                        const float velocity[3],
                                        const float wishdir[3],
                                        const float wishspeed,
                                        const float accel,
                                        const float frametime);
void StrafeHelper_Draw(const struct StrafeHelperParams *params,
                       const float hud_width, const float hud_height);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* STRAFE_HELPER_H */
