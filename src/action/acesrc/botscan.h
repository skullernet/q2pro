/*
 * $Header: /LicenseToKill/src/acesrc/botscan.h 1     16/10/99 8:41 Riever $
 *
 * $Log: /LicenseToKill/src/acesrc/botscan.h $
 * 
 * 1     16/10/99 8:41 Riever
 * Initial import to LTK
 * 
 * 1     14/10/99 8:26 Riever
 * 
 * 3     14/10/99 6:53 Riever
 * Changed file name to botscan.h to make it clear where it belongs in the
 * project.
 * 
 * 2     14/10/99 6:51 Riever
 * First version.
 * 
 */
//=================================================================
// botscan.h
//
// Connor Caple 14th October 1999
//
// A lexical scanner module to allow much more complex configuration files
// Original code idea : Lee & Mark Atkinson, "Using C", pub. Que
//
//=================================================================

#define LEXERR	0
#define SYMBOL	1
#define INTLIT	2
#define REALLIT 3
#define STRLIT	4
#define LPAREN	5
#define RPAREN	6
#define SEMIC	7
#define COLON	8
#define COMMA	9
#define PERIOD	10
#define APOST	11
#define PLUSOP	12
#define MINUSOP 13
#define MUXOP	14
#define DIVOP	15
#define	POWOP	16
#define	ASSIGNOP	17
#define HASH	18
#define BANG	19
#define	EOL		20
#define UNDEF	255

void	scanner( char **text, char *token, int *ttype);
char	*nmtoken( int ttype );
