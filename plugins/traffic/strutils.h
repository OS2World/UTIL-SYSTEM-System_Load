#ifndef STRUTILS_H
#define STRUTILS_H

#define _STR_skipSpaces(s) do { while( isspace( *s ) ) s++; } while( 0 )
#define _STR_skipRSpaces(s,e) while( ( e > (s) ) && ( isspace(*(e-1)) ) ) e--
#define _STR_skipDelim(s,d) while( ( *s == d ) || isspace(*s) ) s++
#define _STR_skipNotDelim(sz,d) \
  while( ( *s != '\0' ) && ( d == ' ' ? !isspace(*s) : *s != d ) ) s++
#define _STR_stricmp(s1,s2) \
  stricmp( (s1) == NULL ? "" : (s1), (s2) == NULL ? "" : (s2) )
#define _STR_strlen(s) ( (s) == NULL ? 0 : strlen(s) )

#endif // STRUTILS_H

