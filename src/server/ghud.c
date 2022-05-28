#include "server.h"

#ifdef AQTION_EXTENSION


void SV_Ghud_Clear()
{
	memset(&svs.ghud, 0, sizeof(svs.ghud));
}


void SV_Ghud_SendUpdateToClient(client_t *client)
{
	if (client->protocol == PROTOCOL_VERSION_AQTION && client->version >= PROTOCOL_VERSION_AQTION_GHUD)
	{
		MSG_WriteByte(svc_ghudupdate);
		qboolean written = false;
		int i;
		for (i = 0; i < MAX_GHUDS; i++)
		{
			if (!client->ghud_updateflags[i])
				continue;

			ghud_element_t *element = &svs.ghud[i];
			int eflags = element->flags | client->ghud_forceflags[i];

			if (eflags & GHF_HIDE && !(client->ghud_updateflags[i] & GHU_FORCE))
				continue;

			written = true;

			MSG_WriteByte(i);
			MSG_WriteGhud(element, client->ghud_updateflags[i], eflags);

			client->ghud_updateflags[i] = 0;
		}

		if (written)
		{
			MSG_WriteByte(255);
			SV_ClientAddMessage(client, MSG_RELIABLE);
		}
		else
			msg_write.cursize--;

		SZ_Clear(&msg_write);
	}
}


void SV_Ghud_UpdateFlags(int i, int flags)
{
	client_t *client;
	FOR_EACH_CLIENT(client) {
		client->ghud_updateflags[i] |= flags;
	}
}


void SV_Ghud_UnicastSetFlags(edict_t *ent, int i, int flags)
{
	client_t *client;
	FOR_EACH_CLIENT(client) {
		
		if (client->edict != ent)
			continue;

		if (client->ghud_forceflags[i] == flags)
			continue;

		if ((client->ghud_forceflags[i] & GHF_HIDE && !(flags & GHF_HIDE)) || (!(client->ghud_forceflags[i] & GHF_HIDE) && flags & GHF_HIDE))
			client->ghud_updateflags[i] |= GHU_FORCE;

		client->ghud_forceflags[i] = flags;
		client->ghud_updateflags[i] |= GHU_FLAGS;
	}
}


int SV_Ghud_NewElement(int type)
{
	int i;
	for (i = 0; i < MAX_GHUDS; i++)
	{
		if (svs.ghud[i].flags & GHF_INUSE)
			continue;

		break;
	}


	svs.ghud[i].flags = GHF_INUSE;
	svs.ghud[i].type = type;

	SV_Ghud_SetColor(i, 255, 255, 255, 255);
	SV_Ghud_UpdateFlags(i, (GHU_FLAGS | GHU_TYPE));

	return i;
}




void SV_Ghud_SetFlags(int i, int val)
{
	ghud_element_t *element = &svs.ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if ((val & ~GHF_INUSE) == (element->flags & ~GHF_INUSE))
		return;

	if ((element->flags & GHF_HIDE && ~val & GHF_HIDE) || (~element->flags & GHF_HIDE && val & GHF_HIDE))
		val |= GHU_FORCE;

	element->flags = val | GHF_INUSE;

	SV_Ghud_UpdateFlags(i, GHU_FLAGS);
}

void SV_Ghud_SetText(int i, char *text)
{
	ghud_element_t *element = &svs.ghud[i];


	if (!(element->flags & GHF_INUSE))
		return;

	if (strcmp(text, element->text) == 0)
		return;

	strcpy(element->text, text);
	
	SV_Ghud_UpdateFlags(i, GHU_TEXT);
}

void SV_Ghud_SetInt(int i, int val)
{
	ghud_element_t *element = &svs.ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if (val == element->val)
		return;

	element->val = val;
	
	SV_Ghud_UpdateFlags(i, GHU_INT);
}

void SV_Ghud_SetPosition(int i, int x, int y, int z)
{
	ghud_element_t *element = &svs.ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if (x == element->pos[0] && y == element->pos[1])
		return;

	element->pos[0] = x;
	element->pos[1] = y;
	element->pos[2] = z;
	
	SV_Ghud_UpdateFlags(i, GHU_POS);
}

void SV_Ghud_SetAnchor(int i, float x, float y)
{
	ghud_element_t *element = &svs.ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if (x == element->anchor[0] && y == element->anchor[1])
		return;

	element->anchor[0] = x;
	element->anchor[1] = y;
	
	SV_Ghud_UpdateFlags(i, GHU_POS);
}

void SV_Ghud_SetSize(int i, int x, int y)
{
	ghud_element_t *element = &svs.ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	if (x == element->size[0] && y == element->size[1])
		return;

	element->size[0] = x;
	element->size[1] = y;

	SV_Ghud_UpdateFlags(i, GHU_SIZE);
}

void SV_Ghud_SetColor(int i, int r, int g, int b, int a)
{
	ghud_element_t *element = &svs.ghud[i];

	if (!(element->flags & GHF_INUSE))
		return;

	element->color[0] = r;
	element->color[1] = g;
	element->color[2] = b;
	element->color[3] = a;
	
	SV_Ghud_UpdateFlags(i, GHU_COLOR);
}

#endif