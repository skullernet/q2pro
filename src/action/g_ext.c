

//
// Reki
// API extensions from AQTION_EXTENSION
//

#include "g_local.h"
#include "q_ghud.h"

#ifdef AQTION_EXTENSION
//
// Reki
// Setup struct and macro for defining engine-callable entrypoints
// basically a copy-paste of the same system for game-to-engine calls,
// just in reverse
typedef struct extension_func_s
{
	char		name[MAX_QPATH];
	void*		func;
	struct extension_func_s *n;
} extension_func_t;
extension_func_t *g_extension_funcs;

// the do {} while here is a bizarre C-ism to allow for our local variable, probably not the best way to do this
#define g_addextension(ename, efunc) \
				do { \
				extension_func_t *ext = gi.TagMalloc(sizeof(extension_func_t), TAG_GAME); \
				strcpy(ext->name, ename); \
				ext->func = efunc; \
				ext->n = g_extension_funcs; \
				g_extension_funcs = ext; \
				} while (0);

//
// declare engine trap pointers, to make the compiler happy
int(*engine_Client_GetVersion)(edict_t *ent);
int(*engine_Client_GetProtocol)(edict_t *ent);

void(*engine_Ghud_SendUpdates)(edict_t *ent);
int(*engine_Ghud_NewElement)(int type);
void(*engine_Ghud_SetFlags)(int i, int val);
void(*engine_Ghud_UnicastSetFlags)(edict_t *ent, int i, int val);
void(*engine_Ghud_SetInt)(int i, int val);
void(*engine_Ghud_SetText)(int i, char *text);
void(*engine_Ghud_SetPosition)(int i, int x, int y, int z);
void(*engine_Ghud_SetAnchor)(int i, float x, float y);
void(*engine_Ghud_SetColor)(int i, int r, int g, int b, int a);
void(*engine_Ghud_SetSize)(int i, int x, int y);

//
// optional new entrypoints the engine may want to call
edict_t *xerp_ent;
trace_t q_gameabi XERP_trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	return gi.trace(start, mins, maxs, end, xerp_ent, MASK_PLAYERSOLID);
}

int G_customizeentityforclient(edict_t *client, edict_t *ent, entity_state_t *state)
{
	// first check visibility masks
	if (!(max(1, ent->dimension_visible) & max(1, client->client->dimension_observe)))
		return false;
	
	// extrapolation, if we want that kind of thing, which we pretty much don't because antilag exists.
	// this could be used in future if antilag is disabled.
#if 0
	if (ent->client) // extrapolate clients
	{
		if (sv_antilag_interp->value) // don't extrapolate when our antilag accounts for lerp
			return true;

		float xerp_amount = FRAMETIME;
		if (!sv_antilag->value) // we don't add extra xerp with antilag
			xerp_amount += client->client->ping / 1000;

		pmove_t pm;
		xerp_ent = ent;
		memcpy(&pm.s, &ent->client->ps.pmove, sizeof(pmove_state_t));

		pm.trace = XERP_trace;
		pm.pointcontents = gi.pointcontents;
		pm.s.pm_type = PM_NORMAL;

		pm.cmd = ent->client->cmd_last; // just assume client is gonna keep pressing the same buttons for next frame
		// we need to reign in A/D spamming at low velocities
		if ((VectorLength(pm.s.velocity)/8) < 280)
		{
			pm.cmd.sidemove = 0;
			pm.cmd.forwardmove = 0;
		}

		pm.cmd.msec = xerp_amount * 1000;

		gi.Pmove(&pm);

		VectorScale(pm.s.origin, 0.125, state->origin);
	}
	else // extrapolate other physics entities
	{
		if (ent->movetype)
		{
			vec3_t start, end, velocity;
			VectorCopy(state->origin, start);
			VectorCopy(ent->velocity, velocity);

			switch (ent->movetype)
			{
			case MOVETYPE_BOUNCE:
			case MOVETYPE_TOSS:
				velocity[2] -= ent->gravity * sv_gravity->value * FRAMETIME;
			case MOVETYPE_FLY:
				VectorMA(start, FRAMETIME, velocity, end);
				break;
			default:
				return true;
			}

			trace_t trace;
			trace = gi.trace(start, ent->mins, ent->maxs, end, ent, ent->clipmask ? ent->clipmask : MASK_SOLID);
			VectorCopy(trace.endpos, state->origin);
		}
	}
#endif

	return true;
}


void G_InitExtEntrypoints(void)
{
	g_addextension("customizeentityforclient", G_customizeentityforclient);
}


void* G_FetchGameExtension(char *name)
{
	Com_Printf("Game: G_FetchGameExtension for %s\n", name);
	extension_func_t *ext;
	for (ext = g_extension_funcs; ext != NULL; ext = ext->n)
	{
		if (strcmp(ext->name, name))
			continue;

		return ext->func;
	}

	Com_Printf("Game: Extension not found.\n");
	return NULL;
}






// 
// new engine functions we can call from the game

//
// client network querying
//
int Client_GetVersion(edict_t *ent)
{
	if (!engine_Client_GetVersion)
		return 0;

	return engine_Client_GetVersion(ent);
}


int Client_GetProtocol(edict_t *ent)
{
	if (!engine_Client_GetProtocol)
		return 34;

	return engine_Client_GetProtocol(ent);
}



//
// game hud
//
void Ghud_SendUpdates(edict_t *ent)
{
	if (!engine_Ghud_SendUpdates)
		return;

	engine_Ghud_SendUpdates(ent);
}

int Ghud_NewElement(int type)
{
	if (!engine_Ghud_NewElement)
		return 0;

	return engine_Ghud_NewElement(type);
}

void Ghud_SetFlags(int i, int val)
{
	if (!engine_Ghud_SetFlags)
		return;

	engine_Ghud_SetFlags(i, val);
}

void Ghud_UnicastSetFlags(edict_t *ent, int i, int val)
{
	if (!engine_Ghud_UnicastSetFlags)
		return;

	engine_Ghud_UnicastSetFlags(ent, i, val);
}

void Ghud_SetInt(int i, int val)
{
	if (!engine_Ghud_SetInt)
		return;

	engine_Ghud_SetInt(i, val);
}

void Ghud_SetText(int i, char *text)
{
	if (!engine_Ghud_SetText)
		return;

	engine_Ghud_SetText(i, text);
}

void Ghud_SetPosition(int i, int x, int y)
{
	if (!engine_Ghud_SetPosition)
		return;

	engine_Ghud_SetPosition(i, x, y, 0);
}

void Ghud_SetPosition3D(int i, int x, int y, int z)
{
	if (!engine_Ghud_SetPosition)
		return;

	engine_Ghud_SetPosition(i, x, y, z);
}

void Ghud_SetSize(int i, int x, int y)
{
	if (!engine_Ghud_SetSize)
		return;

	engine_Ghud_SetSize(i, x, y);
}

void Ghud_SetAnchor(int i, float x, float y)
{
	if (!engine_Ghud_SetAnchor)
		return;

	engine_Ghud_SetAnchor(i, x, y);
}

void Ghud_SetColor(int i, int r, int g, int b, int a)
{
	if (!engine_Ghud_SetColor)
		return;

	engine_Ghud_SetColor(i, r, g, b, a);
}


//
// game hud abstractions,
// makes code a bit cleaner
//
int Ghud_AddIcon(int x, int y, int image, int sizex, int sizey)
{
	int index = Ghud_NewElement(GHT_IMG);
	Ghud_SetPosition(index, x, y);
	Ghud_SetSize(index, sizex, sizey);
	Ghud_SetInt(index, image);

	return index;
}


int Ghud_AddText(int x, int y, char *text)
{
	int index = Ghud_NewElement(GHT_TEXT);
	Ghud_SetPosition(index, x, y);
	Ghud_SetText(index, text);

	return index;
}


int Ghud_AddNumber(int x, int y, int value)
{
	int index = Ghud_NewElement(GHT_NUM);
	Ghud_SetPosition(index, x, y);
	Ghud_SetInt(index, value);

	return index;
}

#endif