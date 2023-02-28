#include "server.h"
#ifdef AQTION_EXTENSION


void SV_Ghud_Clear()
{
	client_t *cl;
	FOR_EACH_CLIENT(cl) {
		memset(cl->ghud, 0, sizeof(cl->ghud));
	}
}

void SV_Ghud_ClearForClient(edict_t *ent)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	memset(cl->ghud, 0, sizeof(cl->ghud));
}

int SV_Ghud_NewElement(edict_t *ent, int type)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	int i;
	for (i = 0; i < MAX_GHUDS; i++)
	{
		if (cl->ghud[i].flags & GHF_INUSE)
			continue;

		break;
	}


	cl->ghud[i].flags = GHF_INUSE;
	cl->ghud[i].type = type;

	SV_Ghud_SetColor(ent, i, 255, 255, 255, 255);
	//SV_Ghud_UpdateFlags(ent, i, (GHU_FLAGS | GHU_TYPE));

	return i;
}

void SV_Ghud_RemoveElement(edict_t *ent, int i)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	cl->ghud[i].flags = 0;
}


void SV_Ghud_SetFlags(edict_t *ent, int i, int val)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	ghud_element_t *element = &cl->ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if ((val & ~GHF_INUSE) == (element->flags & ~GHF_INUSE))
		return;

	//if ((element->flags & GHF_HIDE && ~val & GHF_HIDE) || (~element->flags & GHF_HIDE && val & GHF_HIDE))
	//	val |= GHU_FORCE;

	element->flags = val | GHF_INUSE;

	//SV_Ghud_UpdateFlags(i, GHU_FLAGS);
}

void SV_Ghud_SetText(edict_t *ent, int i, char *text)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	ghud_element_t *element = &cl->ghud[i];


	if (!(element->flags & GHF_INUSE))
		return;

	if (strcmp(text, element->text) == 0)
		return;

	strcpy(element->text, text);
	
	//SV_Ghud_UpdateFlags(i, GHU_TEXT);
}

void SV_Ghud_SetInt(edict_t *ent, int i, int val)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	ghud_element_t *element = &cl->ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if (val == element->val)
		return;

	element->val = val;
	
	//SV_Ghud_UpdateFlags(i, GHU_INT);
}

void SV_Ghud_SetPosition(edict_t *ent, int i, int x, int y, int z)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	ghud_element_t *element = &cl->ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if (x == element->pos[0] && y == element->pos[1])
		return;

	element->pos[0] = x;
	element->pos[1] = y;
	element->pos[2] = z;
	
	//SV_Ghud_UpdateFlags(i, GHU_POS);
}

void SV_Ghud_SetAnchor(edict_t *ent, int i, float x, float y)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	ghud_element_t *element = &cl->ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if (x == element->anchor[0] && y == element->anchor[1])
		return;

	element->anchor[0] = x;
	element->anchor[1] = y;
	
	//SV_Ghud_UpdateFlags(i, GHU_POS);
}

void SV_Ghud_SetSize(edict_t *ent, int i, int x, int y)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	ghud_element_t *element = &cl->ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if (x == element->size[0] && y == element->size[1])
		return;

	element->size[0] = x;
	element->size[1] = y;

	//SV_Ghud_UpdateFlags(i, GHU_SIZE);
}

void SV_Ghud_SetColor(edict_t *ent, int i, int r, int g, int b, int a)
{
	client_t    *cl;
	int         clientNum;
	clientNum = NUM_FOR_EDICT(ent) - 1;
	if (clientNum < 0 || clientNum >= sv_maxclients->integer) {
		Com_WPrintf("%s to a non-client %d\n", __func__, clientNum);
	}
	cl = svs.client_pool + clientNum;

	ghud_element_t *element = &cl->ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	element->color[0] = r;
	element->color[1] = g;
	element->color[2] = b;
	element->color[3] = a;
	
	//SV_Ghud_UpdateFlags(i, GHU_COLOR);
}

#endif