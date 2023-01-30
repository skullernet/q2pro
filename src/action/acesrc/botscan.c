/*
 * $Header:$
 *
 * $History:$
 * 
 */
//=================================================================
// botscan.c
//
// Connor Caple 14th October 1999
//
// A lexical scanner module to allow much more complex configuration files
// Original code idea : Lee & Mark Atkinson, "Using C", pub. Que
//
// Sample driver included at bottom of file.
//
//=================================================================

#include	"botscan.h"
#include	<stdlib.h>
#include	<stdio.h>

//==============================
// nmtoken
//==============================
// This is only used in testing. It returns a string that matches
// the type of token returned in the scanner
// Very useful when debugging new configuration files
//
// I may write a stand alone config file tester to ensure that
// people use the instructions correctly.
//
char	*nmtoken( int ttype)
{
	static char	*tokenNames[] = {
		"LEXERR",
		"SYMBOL",
		"INTLIT",
		"REALLIT",
		"STRLIT",
		"LPAREN",
		"RPAREN",
		"SEMIC",
		"COLON",
		"COMMA",
		"PERIOD",
		"APOST",
		"PLUSOP",
		"MINUSOP",
		"MUXOP",
		"DIVOP",
		"POWOP",
		"ASSIGNOP",
		"HASH",
		"BANG",
		"EOL",
		"UNDEF"
	};
	return( tokenNames[ttype] );
}

//=================================
// scanner
//=================================
// This is the actual scanner. It searches for tokens it can
// recognise and returns them in a string with the
// tokentype defined in an integer.
//
// text is the address of the string being worked on
// token is the returned value of the token
// ttype is the integer value of the token type
//

void	scanner( char **text, char *token, int *ttype)
{
	// Skip all whitespace
	for ( ; **text == ' ' || **text == '\t' || **text == '\n' || **text == '\r'; (*text)++ );

	// If the string terminates return EOL
	if( **text == '\0' )
	{
		*ttype = EOL;
		return;
	}

	// SYMBOLS
	if( (**text >='A' && **text <='Z') || (**text >='a' && **text <='z') )
	{
		*ttype = SYMBOL;

		while( 
			(**text >='A' && **text <='Z') || (**text >='a' && **text <='z')
			|| (**text >='0' && **text <='9')
			)
		{
			*token++ = *(*text)++;
		}
		*token = '\0';	// Terminate the string.
		return;
	}

	// STRING LITERALS
	if( **text == '"' )
	{
		*ttype = STRLIT;
		(*text)++;	// Skip first quote.
		while( **text != '"' && **text )
		{
			*token++ = *(*text)++;
		}
		(*text)++;	// Skip last quote.
		*token = '\0';	// Terminate the string.
		return;
	}

	// NUMERICS
	if( **text >= '0' && **text <= '9' )
	{
		*ttype = INTLIT;
		while( **text >= '0' && **text <= '9' )
		{
			*token++ = *(*text)++;
			if( **text == '.' )
			{
				*ttype = REALLIT;
				*token++ = *(*text)++;
			}
			// I left out the 'e' notation part - we don't need it.
		}
		*token = '\0';	// Terminate the string.
		return;
	}

	// PUNCTUATION SECTION
	if( **text == '(' )
	{
		*ttype = LPAREN;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == ')' )
	{
		*ttype = RPAREN;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == ';' )
	{
		*ttype = SEMIC;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == ':' )
	{
		*ttype = COLON;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == ',' )
	{
		*ttype = COMMA;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '.' )
	{
		*ttype = PERIOD;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '\'' )
	{
		*ttype = APOST;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '+' )
	{
		*ttype = PLUSOP;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '-' )
	{
		*ttype = MINUSOP;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '*' )
	{
		*ttype = MUXOP;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '/' )
	{
		*ttype = DIVOP;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '^' )
	{
		*ttype = POWOP;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '=' )
	{
		*ttype = ASSIGNOP;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '#' )
	{
		*ttype = HASH;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}

	if( **text == '!' )
	{
		*ttype = BANG;
		*token++ = *(*text)++;
		*token = '\0';
		return;
	}


	// Nothing matched - it must be an error
	*ttype = LEXERR;
	return;
}

/*
// *************************************************************************
// Sample driver to test this file with
//

void	parseString( char *test )
{
	char	*sp, *tp;
	int		ttype;
	char	token[81];

	sp = test;	// Set up a pointer to the string;
	ttype = UNDEF;	// Signal "no match found yet"

	// Now scan the string and report each token type and value found
	while( ttype != EOL && ttype != LEXERR )
	{
		// Set tp to point to our return string location
		tp = token;
		// Pass in the correct values to scanner()
		scanner( &sp, tp, &ttype );
		// If we are not done, print what we found.
		if( ttype != EOL )
		{
			printf( "Token type = %s, token = %s\n", nmtoken(ttype), token );
		}
	}
}
// **************************************************************************
*/
