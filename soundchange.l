%{
  #include <string.h>

  #include "soundchange.h"
  #include "automaton.h"
  #include "soundchange.tab.h"

  #define YY_USER_INIT line_text = (char *)malloc(line_size * sizeof(char));
  #define YY_USER_ACTION take();
%}

        void take();

        int modtype[256] = {0};
        char *text;
        char *line_text = NULL;
        int line_size = 128, line_length = 0, erase_next = 0;

%x posintstcd stringstcd
%option noyywrap
%%

<posintstcd>[0-9]*	{ yylval.ch = atoi(yytext); return POSINT; }
<posintstcd>[ \t]+	{ }

<stringstcd>.*	{ yylval.str = (char *)strdup(yytext); return STRING; }

^\#.*\n	{ return '\n'; } /* a comment; discard, but make sure to count the line */

^"mod01".*\n	{ int i; for(i=5; i<yyleng; i++) modtype[yytext[i]] = MOD01; return '\n'; }
^"mod10".*\n	{ int i; for(i=5; i<yyleng; i++) modtype[yytext[i]] = MOD10; return '\n'; }
^"mod02".*\n	{ int i; for(i=5; i<yyleng; i++) modtype[yytext[i]] = MOD02; return '\n'; }
^"mod11".*\n	{ int i; for(i=5; i<yyleng; i++) modtype[yytext[i]] = MOD11; return '\n'; } /*
make sure lines of this form are counted as lines in the parser.  Also, I've discarded mod20.
 */

"[sporadic"	{ return SPORADIC; }
"[ignore-conflicts"	{ return IGNORE_CONFLICTS; }
"[flip-conflicts"	{ return FLIP_CONFLICTS; }
"[undelete"	{ BEGIN(posintstcd); return UNDELETE; }
"[name"	{ BEGIN(stringstcd); return NAME; }

^[^ \t\n]+[ \t]*\=	{ /* a class definition -- maybe wants a start condition */
          text = (char *)strdup(yytext);
          int i = strspn(text, " \t=");
          int j = strcspn(text+i, " \t=");
          text[i+j] = '\0';
          yylval.str = (char *)strdup(text+i);
          free(text);
          return CLASSDEF;
        }
\[[^\n\]]*\]	{ /* a class reference.  note [ ]; { } are & u\ for X-Sampa compatibility */
          text = (char *)strdup(yytext);
          text[yyleng - 1] = '\0';
          yylval.str = (char *)strdup(text + 1);
          free(text);
          return CLASSREF;
        }

"__"	{ return ENVSPACE; }
"||"	{ return RSPACE; }
[ \t]*"//"	{ return PARALLEL; }
"("	{ return '('; }
")"	{ return ')'; }
"+"	{ return '+'; }
"*"	{ return '*'; } /* also used for constraints! */
"/"	{ return '/'; } /* used in place of ?, which is glottal stop */
"|"     { return '|'; }
[ \t]+	{ return WS; }
.	{
          yylval.ch = yytext[0];
          return modtype[yytext[0]] ? modtype[yytext[0]] : CPHONE;
        }
<*>\n	{ BEGIN(INITIAL); return '\n'; }

%%

void take() {
  char *c;
  if (erase_next) {
    line_length = 0;
    line_text[0] = '\0';
  }
  line_length += yyleng;
  if (line_length >= line_size) {
    line_size += line_length + 1;
    c = line_text;
    line_text = (char *)malloc(line_size * sizeof(char));
    strcpy(line_text, c);
    free(c);
  }
  strcat(line_text, yytext);
  erase_next = (line_text[line_length-1] == '\n');
}
