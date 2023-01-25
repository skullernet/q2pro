//-----------------------------------------------------------------------------
//
//  $Header:$

/*
 * $History:$
 * 
 */
/*
        botchat.c
*/

#include "../g_local.h"

#include "botchat.h"


/*
 *  In each of the following strings:
 *            - the first %s is the attacker/enemy which the string is
 *              directed to
 */

#define DBC_WELCOMES 13
char *ltk_welcomes[DBC_WELCOMES] =
{
        "Greetings all!",
        "Hello %s! Prepare to die!!",
		"%s? Who the hell is that?",
		"I say, %s, have you got a license for that face?",
		"%s, how do you see where you're going with those b*ll*cks in your eyes?",
		"Give your arse a rest, %s. try talking through your mouth",
		"Damn! I hoped Thresh would be here...",
		"Hey Mom, they gave me a gun!",
		"I gotta find a better server...",
		"Hey, any of you guys played before?",
		"Hi %s, I wondered if you'd show your face around here again.",
		"Nice :-) :-) :-)",
		"OK %s, let's get this over with"
};

#define DBC_KILLEDS 13
char *ltk_killeds[DBC_KILLEDS] =
{
        "B*stard! %s messed up my hair.",
        "All right, %s. Now I'm feeling put out!",
        "Hey! Go easy on me! I'm a newbie!",
		"Ooooh, %s, that smarts!",
		"%s's mother cooks socks in hell!",
		"Hey, %s, how about a match - your face and my arse!",
		"Was %s talking to me, or chewing a brick?",
		"Aw, %s doesn't like me...",
		"It's clobberin' time, %s",
		"Hey, I was tying my shoelace!",
		"Oh - now I know how strawberry jam feels...",
		"laaaaaaaaaaaaaaag!",
		"One feels like chicken tonight...."
};

#define DBC_INSULTS 16
char *ltk_insults[DBC_INSULTS] =
{
        "Hey, %s. Your mother was a hamster...!",
        "%s; Eat my dust!",
        "Hahaha! Hook, Line and Sinker, %s!",
		"I'm sorry, %s, did I break your concentration?",
		"Unlike certain other bots, %s, I can kill with an English accent..",
		"Get used to disappointment, %s",
		"You couldn't organise a p*ss-up in a brewery, %s",
		"%s, does your mother know you're out?",
		"Hey, %s, one guy wins, the other prick loses...",
		"Oh %s, ever thought of taking up croquet instead?",
		"Yuck! I've got some %s on my shoe!",
		"Mmmmm... %s chunks!",
		"Hey everyone, %s was better than the Pirates of Penzance",
		"Oh - good play %s ... hehehe",
		"Errm, %s, have you ever thought of taking up croquet instead?",
		"Ooooooh - I'm sooooo scared %s"
};


void LTK_Chat (edict_t *bot, edict_t *object, int speech)
{
        char final[150];
        char *text = NULL;

        if ((!object) || (!object->client))
                return;

        if (speech == DBC_WELCOME)
                text = ltk_welcomes[rand()%DBC_WELCOMES];
        else if (speech == DBC_KILLED)
                text = ltk_killeds[rand()%DBC_KILLEDS];
        else if (speech == DBC_INSULT)
                text = ltk_insults[rand()%DBC_INSULTS];
        else
        {
                if( debug_mode )
                        gi.bprintf (PRINT_HIGH, "LTK_Chat: Unknown speech type attempted!(out of range)");
                return;
        }

        sprintf (final, text, object->client->pers.netname);

        LTK_Say (bot, final);
}


/*
==================
Bot_Say
==================
*/
void LTK_Say (edict_t *ent, char *what)
{
	int		j;
	edict_t	*other;
	char	text[2048];

        Q_snprintf (text, sizeof(text), "%s: ", ent->client->pers.netname);

        if (*what == '"')
        {
                what++;
                what[strlen(what)-1] = 0;
        }
        strcat(text, what);

	// don't let text be too long for malicious reasons
        if (strlen(text) > 200)
                text[200] = '\0';

	strcat(text, "\n");

	if (dedicated->value)
		gi.cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = &g_edicts[j];
		if (!other->inuse)
			continue;
		if (!other->client)
			continue;
        if (Q_stricmp(other->classname, "bot") == 0)
            continue;
		gi.cprintf(other, PRINT_CHAT, "%s", text);
	}
}

