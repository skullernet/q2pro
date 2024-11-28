#include "strafe_helper.h"
#include "strafe_helper_customization.h"
#include "../strafe_helper_includes.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


static float sign(const float value)
{
	if (value < 0.0f)
	{
		return -1.0f;
	}
	else if (value > 0.0f)
	{
		return 1.0f;
	}
	return 0.0f;
}

static float crossProduct(const float v[2], const float w[2])
{
	return v[0] * w[1] - v[1] * w[0];
}

static float dotProduct(const float v[2], const float w[2])
{
	float dot_product = 0.0f;
	int i;
	for (i = 0; i < 2; i += 1) {
		dot_product += v[i] * w[i];
	}
	return dot_product;
}

static float angleBetweenVectors(const float v[2], const float w[2])
{
	return atan2f(crossProduct(v, w), dotProduct(v, w));
}

static float vectorAngleSign(const float v[2], const float w[2])
{
	return sign(crossProduct(v, w));
}

static float vectorNorm(const float v[2])
{
	return sqrtf(dotProduct(v, v));
}


/* The current angle between the players velocity vector and forward-looking
 * vector. */
static float angle_current;
/* The angle between the players velocity vector and forward-looking vector
 * that would have resulted in the maximum amount of acceleration for the
 * last reported acceleration values. */
static float angle_optimal;
/* The (absolute) minimum and maximum angles between the players velocity
 * vector and forward-looking vector that would give some acceleration. */
static float angle_minimum;
static float angle_maximum;

#ifndef STRAFE_HELPER_CUSTOMIZATION_DISABLE_DRAW_SPEED
/* The current player speed in the horizontal plane */
static float speed;
#endif

void StrafeHelper_SetAccelerationValues(const float forward[3],
                                        const float velocity[3],
                                        const float wishdir[3],
                                        const float wishspeed,
                                        const float accel,
                                        const float frametime)
{
	const float v_z = velocity[2];
	const float w_z = wishdir[2];

	const float velocity_norm = vectorNorm(velocity);
	const float wishdir_norm = vectorNorm(wishdir);

	const float forward_velocity_angle = angleBetweenVectors(wishdir, forward);
	const float angle_sign = vectorAngleSign(wishdir, velocity);
	const float two_pi = 2.0f * (float)M_PI;

	angle_optimal = (wishspeed * (1.0f - accel * frametime) - v_z * w_z)
	                / (velocity_norm * wishdir_norm);
	angle_optimal = acosf(angle_optimal);
	angle_optimal = angle_sign * angle_optimal - forward_velocity_angle;

	angle_minimum = (wishspeed - v_z * w_z) / (2.0f - wishdir_norm * wishdir_norm)
	                * wishdir_norm / velocity_norm;
	angle_minimum = acosf(angle_minimum < 1.0f ? angle_minimum : 1.0f);
	angle_minimum = angle_sign * angle_minimum - forward_velocity_angle;

	angle_maximum = -0.5f * accel * frametime * wishspeed * wishdir_norm
	                / velocity_norm;
	angle_maximum = acosf(angle_maximum);
	angle_maximum = angle_sign * angle_maximum - forward_velocity_angle;

	angle_current = angleBetweenVectors(forward, velocity);

	/* Make sure that angle_current fits well to the other angles. That is, try
	 * equivalent angles by adding or subtracting multiples of 2 * M_PI such
	 * that the angle values are closest to each other. That way we avoid
	 * differences greater than 2 * M_PI between the angles, which would break
	 * the drawing code. */
	angle_current += truncf((angle_minimum - angle_current) / two_pi) * two_pi;
	angle_current += truncf((angle_maximum - angle_current) / two_pi) * two_pi;

#ifndef STRAFE_HELPER_CUSTOMIZATION_DISABLE_DRAW_SPEED
	speed = velocity_norm;
#endif
}


static float angleDiffToPixelDiff(const float angle_difference, const float scale,
                                  const float hud_width)
{
	return angle_difference * (hud_width / 2.0f) * scale / (float)M_PI;
}

static float angleToPixel(const float angle, const float scale,
                          const float hud_width)
{
	return (hud_width / 2.0f) - 0.5f +
	       angleDiffToPixelDiff(angle, scale, hud_width);
}

void StrafeHelper_Draw(const struct StrafeHelperParams *params,
                       const float hud_width, const float hud_height)
{
	float angle_x;
	float angle_width;
#ifndef STRAFE_HELPER_CUSTOMIZATION_DISABLE_DRAW_SPEED
	char speed_string[8];  /* 7 digits should be more than enough for speed */
#endif

	const float upper_y = (hud_height - params->height) / 2.0f + params->y;

	float offset = 0.0f;
	if (params->center)	{
		offset = -angle_current;
	}

	if (angle_minimum < angle_maximum) {
		angle_x = angle_minimum + offset;
		angle_width = angle_maximum - angle_minimum;
	} else {
		angle_x = angle_maximum + offset;
		angle_width = angle_minimum - angle_maximum;
	}

	shc_drawFilledRectangle(
		angleToPixel(angle_x, params->scale, hud_width), upper_y,
		angleDiffToPixelDiff(angle_width, params->scale, hud_width),
		params->height, shc_ElementId_AcceleratingAngles);
	shc_drawFilledRectangle(
		angleToPixel(angle_optimal + offset, params->scale, hud_width) - 0.5f,
		upper_y, 2.0f, params->height, shc_ElementId_OptimalAngle);
	if (params->center_marker) {
		shc_drawFilledRectangle(
			angleToPixel(angle_current + offset, params->scale, hud_width) - 0.5f,
			upper_y + params->height / 2.0f, 2.0f, params->height / 2.0f,
			shc_ElementId_CenterMarker);
	}

#ifndef STRAFE_HELPER_CUSTOMIZATION_DISABLE_DRAW_SPEED
	if (params->speed_scale > 0.0f) {
		snprintf(speed_string, sizeof(speed_string), "%.0f", speed);
		shc_drawString(
			hud_width / 2.0f + params->speed_x,
			upper_y + params->height + params->speed_y,
			speed_string, params->speed_scale, shc_ElementId_SpeedText);
	}
#endif
}
