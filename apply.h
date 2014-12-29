#ifndef __RSCA_APPLY
#define __RSCA_APPLY

#include <stdio.h>

#include "automaton.h"
#include "soundchange.h"
#include "soundchange.tab.h"

extern FILE *yyin;
extern int yyparse();
extern int modtype[256];
extern vector<automaton *> changes;
extern vector<change_parameters *> change_stuff;

vector<string> *tokenise(string s);
void apply_changes();
bool handle_args(int argv, char **argc);
int main(int argv, char **argc);

char *filename = NULL;
bool debug_changes = false;
bool debug_automata = false;
bool reverse_changes = false;
bool complaint = true;
bool display_brackets = false;
bool display_wedges = false;
bool first_word_only = false;

#endif
