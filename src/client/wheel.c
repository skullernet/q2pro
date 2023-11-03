/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "client.h"
#include "common/loc.h"

#define cl_carousel_time 300 // todo cvar

static int sort_wheel_powerups(const void *a, const void *b)
{
    const cl_wheel_powerup_t *pa = a;
    const cl_wheel_powerup_t *pb = b;

    if (pa->sort_id == pb->sort_id)
        return pa->item_index - pb->item_index;

    return pa->sort_id - pb->sort_id;
}

static int sort_wheel_weapons(const void *a, const void *b)
{
    const cl_wheel_weapon_t *pa = a;
    const cl_wheel_weapon_t *pb = b;

    if (pa->sort_id == pb->sort_id)
        return pa->item_index - pb->item_index;

    return pa->sort_id - pb->sort_id;
}

static inline void set_compressed_integer(size_t bits_per_value, uint16_t *start, uint8_t id, uint16_t count)
{
	uint16_t bit_offset = bits_per_value * id;
	uint16_t byte = bit_offset / 8;
	uint16_t bit_shift = bit_offset % 8;
	uint16_t mask = (BIT(bits_per_value) - 1) << bit_shift;
	uint16_t *base = (uint16_t *) ((uint8_t *) start + byte);
	*base = (*base & ~mask) | ((count << bit_shift) & mask);
}

static inline uint16_t get_compressed_integer(size_t bits_per_value, uint16_t *start, uint8_t id)
{
	uint16_t bit_offset = bits_per_value * id;
	uint16_t byte = bit_offset / 8;
	uint16_t bit_shift = bit_offset % 8;
	uint16_t mask = (BIT(bits_per_value) - 1) << bit_shift;
	uint16_t *base = (uint16_t *) ((uint8_t *) start + byte);
	return (*base & mask) >> bit_shift;
}

static inline void G_SetAmmoStat(uint16_t *start, uint8_t ammo_id, uint16_t count)
{
	set_compressed_integer(NUM_BITS_FOR_AMMO, start, ammo_id, count);
}

static inline uint16_t G_GetAmmoStat(uint16_t *start, uint8_t ammo_id)
{
	return get_compressed_integer(NUM_BITS_FOR_AMMO, start, ammo_id);
}

static inline void G_SetPowerupStat(uint16_t *start, uint8_t powerup_id, uint16_t count)
{
	set_compressed_integer(NUM_BITS_PER_POWERUP, start, powerup_id, count);
}

static inline uint16_t G_GetPowerupStat(uint16_t *start, uint8_t powerup_id)
{
	return get_compressed_integer(NUM_BITS_PER_POWERUP, start, powerup_id);
}

void CL_Carousel_Close(void)
{
	cl.carousel.awaiting_close = false;
	cl.carousel.open = false;
}

// populate slot list with stuff we own.
// runs every frame and when we open the carousel.
static bool CL_Carousel_Populate(void)
{
	int i;

	cl.carousel.num_slots = 0;

	int owned = cgame->GetOwnedWeaponWheelWeapons(&cl.frame.ps);

	for (i = 0; i < cl.wheel.num_weapons; i++) {
		if (!(owned & BIT(i)))
			continue;

		cl.carousel.slots[cl.carousel.num_slots].data_id = i;
		cl.carousel.slots[cl.carousel.num_slots].has_ammo = cl.wheel.weapons[i].ammo_index == -1 || cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, cl.wheel.weapons[i].ammo_index);
		cl.carousel.slots[cl.carousel.num_slots].item_index = cl.wheel.weapons[i].item_index;
		cl.carousel.slots[cl.carousel.num_slots].is_powerup = false;
		cl.carousel.num_slots++;
	}

	// todo: sort by sort_id

	// todo: cl.wheel.powerups

	if (!cl.carousel.num_slots)
		return false;

	// check that we still have the item being selected
	if (cl.carousel.selected == -1) {
		cl.carousel.selected = cl.carousel.slots[0].item_index;
	} else {
		for (i = 0; i < cl.carousel.num_slots; i++)
			if (cl.carousel.slots[i].item_index == cl.carousel.selected)
				break;
	}

	if (i == cl.carousel.num_slots) {
		// TODO: maybe something smarter?
		return false;
	}

	return true;
}

static void CL_Carousel_Open(void)
{
	cl.carousel.open = true;
	cl.carousel.selected = (cl.frame.ps.stats[STAT_ACTIVE_WEAPON] == -1) ? -1 : cl.wheel.weapons[cl.frame.ps.stats[STAT_ACTIVE_WEAPON]].item_index;

	if (!CL_Carousel_Populate()) {
		CL_Carousel_Close();
	}
}

void CL_Carousel_Draw(void)
{
	if (!cl.carousel.open)
		return;

	if (!CL_Carousel_Populate()) {
		CL_Carousel_Close();
		return;
	}

	for (int i = 0, y = 128; i < cl.carousel.num_slots; i++, y += CHAR_HEIGHT) {

		int flags = UI_DROPSHADOW;
		int x = 24;

		if (cl.carousel.selected == cl.carousel.slots[i].item_index) {
			flags |= UI_ALTCOLOR;
			x += 16;
			SCR_DrawString(x - 16, y, flags, ">");
		}

		char localized[64];

		Loc_Localize(cl.configstrings[cl.csr.items + cl.carousel.slots[i].item_index], false, NULL, 0, localized, sizeof(localized));

		SCR_DrawString(x, y, flags, localized);
	}
}

void CL_Carousel_ClearInput(void)
{
	if (cl.carousel.awaiting_close) {
		cl.carousel.awaiting_close = false;
		cl.carousel.open = false;
	}
}

void CL_Carousel_Input(void)
{
	if (!cl.carousel.open || cl.carousel.awaiting_close)
		return;

	if (!CL_Carousel_Populate()) {
		CL_Carousel_Close();
		return;
	}

	// always holster while open
	cl.cmd.buttons |= BUTTON_HOLSTER;

	if (cls.realtime >= cl.carousel.close_time || (cl.cmd.buttons & BUTTON_ATTACK)) {

		// already using this weapon
		if (cl.carousel.selected == cl.wheel.weapons[cl.frame.ps.stats[STAT_ACTIVE_WEAPON]].item_index) {
			CL_Carousel_Close();
			return;
		}

		// switch
		CL_ClientCommand(va("use_index_only %i\n", cl.carousel.selected));
		cl.carousel.awaiting_close = true;
	}
}

void CL_Wheel_WeapNext(void)
{
	if (!cl.carousel.open) {
		CL_Carousel_Open();
	} else if (!CL_Carousel_Populate()) {
		CL_Carousel_Close();
		return;
	}

	for (int i = 0; i < cl.carousel.num_slots; i++)
		if (cl.carousel.slots[i].item_index == cl.carousel.selected) {
			cl.carousel.selected = cl.carousel.slots[i == cl.carousel.num_slots - 1 ? 0 : (i + 1)].item_index;
			break;
		}

	cl.carousel.close_time = cls.realtime + cl_carousel_time;
}

void CL_Wheel_WeapPrev(void)
{
	if (!cl.carousel.open) {
		CL_Carousel_Open();
	} else if (!CL_Carousel_Populate()) {
		CL_Carousel_Close();
		return;
	}

	for (int i = 0; i < cl.carousel.num_slots; i++)
		if (cl.carousel.slots[i].item_index == cl.carousel.selected) {
			cl.carousel.selected = cl.carousel.slots[i == 0 ? cl.carousel.num_slots - 1 : (i - 1)].item_index;
			break;
		}

	cl.carousel.close_time = cls.realtime + cl_carousel_time;
}
