%{
  #include <stdlib.h>
  #include <stdio.h>
  #include <string.h>
  #include <string>
  #include <vector>
  #include <map>
  #include <set>
  #include <algorithm>

  using namespace std;

  #include "soundchange.h"
  #include "automaton.h"
  
  extern int yylex();
  extern char *yytext;
  extern char *line_text;

  extern char *filename;
  extern bool debug_automata;
  extern bool reverse_changes;
  
  int yyparse();
  void yyerror(char *s);
  void check_category(string s);
  void add_presentable_name(char *s);
  automaton *interpret_classref(char *p, int group = -1);
  automaton *glue(vector<transition *> *r, vector<transition *> *dr);
  void glue1(vector<transition *> *r, vector<transition *> *dr, automaton *a,
                   int &i, int &j, int ii, int jj);
  automaton *split(automaton *a);
  void reachable_excluding(automaton *a, set<int> *s, int x, int y, int z);
  vector<string> *corresponding_phoneset(vector<string> *v, string s0, string s1, int group);

  vector<automaton *> changes;
  vector<change_parameters *> change_stuff;
  map<string, vector<string>*> category;
  map<int, string> split_category;
  string current_name;
  int current_automaton_sort = -1; 
  int line = 1;
%}

%start foo

%union {
  int ch;
  char *str;
  automaton *autp;
  change_parameters *parp;
  vector<string> *vbstrp;
  vector<transition *> *rautp;
  vector<automaton *> *vautpp;
  vector<vector<transition *> *> *vrautpp;
}

%token <ch> WS CPHONE MOD01 MOD10 MOD02 MOD11 ENVSPACE RSPACE PARALLEL POSINT
SPORADIC UNDELETE NAME IGNORE_CONFLICTS FLIP_CONFLICTS
%token <str> CLASSDEF CLASSREF STRING

%type <str> phone
%type <autp> env_part nil_or_env soundchange_strand soundchange_strands
%type <parp> parameter_list
%type <vautpp> nil_or_envs
%type <rautp> renv_part drenv_part
%type <vrautpp> renv_parts drenv_parts
%type <vbstrp> phone_set

%left '|'
%left CATENATION

%%

foo: category_list soundchange_list {
          /* Don't forget about this bit of code!  It's really quite important.  */
          if (reverse_changes) {
            reverse(changes.begin(), changes.end());
            reverse(change_stuff.begin(), change_stuff.end());
          }
        }
;

category_list:     /* nil */ { }
        | newline category_list { }
        | CLASSDEF phone_set newline category_list {
          category[string($1)] = $2;
          //printf("defined category %s\n", $1);
          ($1);
        } 
;

phone_set: opt_ws            {
          $$ = new vector<string>;
        }
        | opt_ws phone phone_set {
          $$ = $3;
          $$->push_back(string($2));
        }
;

soundchange_list: /* nil */  { }
        | opt_ws newline soundchange_list { }
        | soundchange opt_ws newline soundchange_list { }
;



soundchange: parameter_list opt_ws soundchange_strands {
          if ($1->reflect)
            $3->reflect();
          automaton *b = $3->determinise($1->not_sporadic, current_automaton_sort, $1->respecting_conflicts);
          if (b == NULL) {
            fprintf(stderr, "%s:%d: conflict in determinisation (sound change may have ambiguous cases)\n",
                    filename, line);
            exit(1);
          }
          current_automaton_sort = -1;
          if (debug_automata) {
            printf("after determinise\n");
            b->display();
          }
        
          if (reverse_changes) {
            if (!b->invert()) {
              fprintf(stderr, "%s:%d: change cannot be reversed\n",
                      filename, line);
              exit(1);
            }
            if (debug_automata) {
              printf("after reversal\n");
              b->display();
            }
          }
        
          // to an automaton whose transitions aren't being used elsewhere, do this:
          $3->free_transitions(); $3;
          changes.push_back(b);  

          /* Prepare the parameters of this change.  */
          if($1->name == "")
            $1->name = current_name;
          change_stuff.push_back($1);

          current_name = "";
        }
;


parameter_list: /* nil */ { $$ = new change_parameters(); }
        | opt_ws SPORADIC opt_ws newline parameter_list { $$ = $5; $$->not_sporadic = false; }
        | opt_ws IGNORE_CONFLICTS opt_ws newline parameter_list { $$ = $5; $$->respecting_conflicts = false; }
        | opt_ws FLIP_CONFLICTS opt_ws newline parameter_list { $$ = $5; $$->reflect = true; }
        | opt_ws UNDELETE POSINT newline parameter_list { $$ = $5; $$->max_epen = $3; }
        | opt_ws NAME STRING newline parameter_list {
          $$ = $5;
          char *c = $3;
          while (*c == ' ' || *c == '\t') c++;
          $$->name = string(c);
          $$->name.resize(1 + $$->name.find_last_not_of(" \t"));
          //($3);
        }
;

soundchange_strands: soundchange_strand {
          $$ = $1;
        }
        | soundchange_strand PARALLEL opt_ws newline opt_ws soundchange_strands {
          $$ = $1;
          $$->alternate($6);
          $6;
        }
;

soundchange_strand: renv_parts WS drenv_parts WS nil_or_envs {
          /* Changes and constraints cannot be parallelized together, since the
             determinization wouldn't have a well-defined initial form (let alone
             that it's simply a silly thing to do).  This handling is klugish, 
             but leaves the grammar a bit less reusy, and gives an informative
             error message.  */
          if (current_automaton_sort == 2) {
            fprintf(stderr, "%s:%d: can't parallelise changes and constraints\n", filename, line);
            exit(1);
          }
          current_automaton_sort = 0;

          /* We don't need to test for the case of one environment part and zero changes,
             because the latter is unspecifiable.  */
          if ($1->size() != $3->size()) {
            fprintf(stderr, "%s:%d: number of befores doesn't equal number of afters\n", filename, line);
            exit(1);
          }
          if ($1->size() != $5->size()-1) {
            fprintf(stderr, "%s:%d: number of changes doesn't match number of environments\n", filename, line);
            exit(1);
          }
        
          automaton *a0 = new automaton(1), *a;
          for(int i=$5->size()-1; i>=0; i--) {
            a0->catenate((*$5)[i]);
            (*$5)[i];
            if (i>=1) {
              a = glue((*$1)[i-1], (*$3)[i-1]);
              (*$1)[i-1];
              (*$3)[i-1];
              a0->catenate(a);
              a;
            }
          }
        
          $$ = split(a0);
        
          if (debug_automata) {
            printf("as written\n");
            a0->display();
            printf("after split\n");
            $$->display();
          }
        
          // to an automaton whose transitions aren't being used elsewhere, do this:
          a0->free_transitions(); a0;

          split_category.clear();

          add_presentable_name(line_text);
        }
        | '*' opt_ws env_part {
          if (current_automaton_sort == 0) {
            fprintf(stderr, "%s:%d: can't parallelise constraints and changes\n", filename, line);
            exit(1);
          }
          current_automaton_sort = 2;
        
          $$ = split($3);
        
          if (debug_automata) {
            printf("as written\n");
            $3->display();
            printf("after split\n");
            $$->display();
          }
        
          $3->free_transitions(); $3;
        
          split_category.clear();
        
          add_presentable_name(line_text);
        }
;

renv_parts: renv_part {
          $$ = new vector<vector<transition *> *>;
          $$->push_back($1);
        }
        | renv_part RSPACE renv_parts {
          $$ = $3;
          $$->push_back($1);
        }
;

drenv_parts: drenv_part {
          $$ = new vector<vector<transition *> *>;
          $$->push_back($1);
        }
        | drenv_part RSPACE drenv_parts {
          $$ = $3;
          $$->push_back($1);
        }
;

renv_part: /* nil */ {
          $$ = new vector<transition *>;
        }
        | phone renv_part {
          $$ = $2;
          $$->push_back(new cst_transition(string($1)));
        }
        | CLASSREF renv_part {
          automaton *c = interpret_classref($1, 0); // 0 is the default split-group in before
          $$ = $2;
          
          $$->push_back(c->q[0].t[0]);
          (c);
          ($1);
        }
;

drenv_part: /* nil */ {
          $$ = new vector<transition *>;
        }
        | phone drenv_part {
          $$ = $2;
          $$->push_back(new cst_transition(string($1)));
        }
        | CLASSREF drenv_part {
          char *u0 = $1;
          // default to split-group 0, which is also default for every grouping in before
          int bun = 0;
        
          $$ = $2;
        
          if(*$1 == '#') {
            /* There is an explicit split-group reference; parse it out. */
            strsep(&u0, " \t");
            bun = atoi($1+1);
          }
          
          if (!u0) 
            $$->push_back(new spl_transition("", bun));
          else {
            $$->push_back(new spl_transition(string(u0), bun));
          }
          ($1);
        }
;

nil_or_envs: nil_or_env {
          $$ = new vector<automaton *>;
          $$->push_back($1);
        }
        | nil_or_env ENVSPACE nil_or_envs {
          $$ = $3;
          $$->push_back($1);
        }
;

nil_or_env: /* nil */                           { $$ = new automaton(1); }
        | env_part                              { $$ = $1; }
;

env_part: phone                                 { $$ = new automaton($1); }
        | CLASSREF                              {
          $$ = interpret_classref($1);
          ($1);
        }
        | env_part '|' env_part                 { $$ = $1; $$->alternate($3); $3; }
        | env_part env_part %prec CATENATION    { $$ = $1; $$->catenate($2); $2; }
        | env_part '*'                          { $$->kleene_star(); }
        | env_part '+'                          { $$->kleene_plus(); }
        | env_part '/'                          { $$->optionalize(); }
        | '(' env_part ')'                      { $$ = $2; }
;

phone: 	  CPHONE		{
          char *a = (char *)malloc(2);
          a[1] = '\0'; a[0] = $1; 
          $$ = a;
        }
        | phone MOD11 phone	{
          char *a = (char *)malloc(2+strlen($1)+strlen($3));
          strcpy(a, $1);
          a[1+strlen(a)] = '\0'; a[strlen(a)] = $2;
          strcat(a, $3);
          ($1); ($3);
          $$ = a;
        }
        | phone MOD10		{
          char *a = (char *)malloc(2+strlen($1));
          strcpy(a, $1);
          a[1+strlen(a)] = '\0'; a[strlen(a)] = $2;
          ($1);
          $$ = a;
        }
        | MOD02 phone phone	{
          char *a = (char *)malloc(2+strlen($2)+strlen($3));
          a[0] = $1;
          strcpy(a+1, $2);
          strcat(a, $3);
          ($2); ($3);
          $$ = a;  
        }
        | MOD01 phone		{
          char *a = (char *)malloc(2+strlen($2));
          a[0] = $1;
          strcpy(a+1, $2);
          ($2);
          $$ = a;
        }
/*        | phone phone MOD20	{
          char *a = (char *)malloc(2+strlen($1)+strlen($2));
          strcpy(a, $1);
          strcat(a, $2);
          a[1+strlen(a)] = '\0'; a[strlen(a)] = $3;
          ($1); ($2);
          $$ = a;
        }*/
;

newline:  '\n'                  { line++; }
;

opt_ws:   /* nil */
        | WS            { }
;

%%







/* Tolerating these errors: we can try, if we set up some exceptionish mechanism.  */
void yyerror (char *s) {
  fprintf(stderr, "%s:%d: %s at or before ", filename, line, s);
  if (yytext[0]=='\n')
    fprintf(stderr, "end of line\n");
  else
    fprintf(stderr, "\"%s\"\n", yytext);
  exit(1);
}

/* I'd like to generalize the whole error reporting mechanism towards the end
   of providing some recoverability, but for this I first want to understand
   variadic functions.  */

/* Barf if an undefined category name is used.  Also handle special categories
   of the form ^s, consisting of the single phone s, so that e.g.
   [vlstop ^^t] specifies vlstops other than t.  */
void check_category(string s) {
  if (category.find(s) == category.end()) {
    if (s[0] == '^') {
      vector<string> *v = new vector<string>(1, s.substr(1));
      category[s] = v;
    }
    else {
      fprintf(stderr, "%s:%d: undefined category \"%s\"\n", filename, line, s.c_str());
      exit(1);
    }
  }
}

/* Add the name of one strand of the current soundchange to the global
   current_name, after normalizing it wrt whitespace.  */
void add_presentable_name(char *s) {
  /* Simply copy individual characters manually, twiddling the whitespace
     as need be.  */
  bool just_saw_graphic = false;
  for(; *s; s++) {
    if (*s == ' ' || *s == '\t' || *s == '\n') {
      if (just_saw_graphic)
        current_name += ' ';
      just_saw_graphic = false;
    }
    else {
      current_name += *s;
      just_saw_graphic = true;
    }
  }
  /* Check for a parallelization operator.  There should be a space at the end
     exactly if there is one.  */
  if (current_name[current_name.size()-1] == '/' && current_name[current_name.size()-2] == '/')
    current_name += ' ';
  else if (current_name[current_name.size()-1] == ' ')
    current_name.resize(current_name.size()-1);
}

/* Given a string containing multiple whitespace-delimited category names,
   return an automaton which transitions on the intersection of
   all of those categories.
   The category names can be preceded by a ^, which complements them. */
automaton *interpret_classref(char *p, int group) {
  /* s contains the phones matched or not matched, according to the value of s_pos. */
  forfc<string> s("#", false);
  bool first = true;
  
  char *q = strdup(p), *q0 = q, *q1;
  while(q1 = strsep(&q0, " \t")) {
    forfc<string> t;
    if (!*q1) continue;
    /* Handle split-group definitions.  */
    if (strspn(q1, "0123456789") == strlen(q1)) {
      group = atoi(q1);
      continue;
    }
    /* Handle split-group references.  */
    else if (*q1 == '#') {
      int bun = atoi(q1+1);

      string s;
      if (!q0)
        s = "";
      else
        s = string(q0);
      automaton *a = new automaton(new spl_transition(s, bun));
      (q);
      return a;
    }
    else if (*q1 == '^') {
      check_category(string(q1+1));
      t = forfc<string>(*category[string(q1+1)], false); 
    }
    else {
      check_category(string(q1));
      t = forfc<string>(*category[string(q1)], true);
    }

    /* Remember the first category name for this split group, to use
       when splitting.  If there is a group number, it must precede
       this.  If this is negated, things will fail down the line.  */
    if (first) {
      first = false;
      split_category[group] = string(q1);
    }

    s.intersect(t);
  }

  (q);
  return new automaton(&s.s, s.pos, group);
}

/* Stick together vectors of before and after transitions, of categories
   renv_part and drenv_part respectively, into an automaton.  To do
   this, first try to line up any split-group references for which
   this is possible with their definitions.  Then simply pair up
   the others into proper transitions.

   Note that this won't allow some instances of multiple split-class
   definitions to be caught.  */
automaton *glue(vector<transition *> *r, vector<transition *> *dr) {
  automaton *a = new automaton(1); // starts out empty
  /* These indices must be globally maintained.  */
  int ii_ = r->size()-1, jj = dr->size()-1, ii; // for lining up in r and dr
  int i = ii_, j = jj; // for between lining up
  int group;
  
  /* Scan for split-group references in the output which have
     a correspondant in the input among transitions we haven't
     seen yet.  */
  for(; jj >= 0; jj--) 
    if((*dr)[jj]->kind() == SPL_TR) {
      //printf("jj=%d\n", jj);
      for(ii = ii_; ii >= 0; ii--) 
        if ((group = (*r)[ii]->sgd) == ((spl_transition *)(*dr)[jj])->bun) {
          //printf("ii=%d\n", ii);
          /* We found one.  First append all the transitions before it,
             and update i and j to skip this transition. */
          glue1(r, dr, a, i, j, ii, jj);
          i--;
          j--;
          
          /* Append the transition given by ii and jj.  For this we have
             to interpret categories.  First check that the categories correspond.  */
          string s0 = split_category[group];
          string s1 = ((splitting_transition *)(*dr)[jj])->h;
          vector<string> *v = &((cst_transition *)(*r)[ii])->x;
          vector<string> *w = corresponding_phoneset(v, s0, s1, group);

          automaton *b = new automaton(v, w);
          a->catenate(b);
          b;
          w;
          break;
        } // if kind of ii
      /* If we found one, we've now processed everything up to it; take note of this.  */
      if (ii >= 0)
        ii_ = ii-1;
    } // if kind of jj
  /* Finally, append all the transitions after the last alignment.  */
  glue1(r, dr, a, i, j, -1, -1);
      
  return a;
}

/* Do the same sticking together as glue, but without the need to worry
   that there may be things to align.  */
void glue1(vector<transition *> *r, vector<transition *> *dr, automaton *a,
                 int &i, int &j, int ii, int jj) {

  /* It always suffices to glue half-transitions to zeros.  We can actually
     glue two nonzero half-transitions in certain cases.  */
  while(i>ii || j>jj) {
    transition *t;

    if (i>ii && (*r)[i]->kind() == SPL_TR) {
      string s1 = "0";
      if (j>jj && (*dr)[j]->kind() == CST_TR) {
        s1 = ((cst_transition *)(*dr)[j])->x[0];
        j--;
      }      
      t = new rspl_transition(((spl_transition *)(*r)[i])->h, ((spl_transition *)(*r)[i])->bun, s1);
      i--;
    }
    else if (i>ii && (*r)[i]->kind() == NEG_TR) { // always has a split-group
      string s1 = "0";
      if (j>jj && (*dr)[j]->kind() == CST_TR) {
        s1 = ((cst_transition *)(*dr)[j])->x[0];
        j--;
      }
      t = new ner_transition(((neg_transition *)(*r)[i])->z, s1);
      t->sgd = (*r)[i]->sgd;
      i--;
    }
    else if (i>ii && (*r)[i]->kind() == CST_TR && (*r)[i]->sgd >= 0) {
      string s1 = "0";
      if (j>jj && (*dr)[j]->kind() == CST_TR) {
        s1 = ((cst_transition *)(*dr)[j])->x[0];
        j--;
      }
      t = new pos_transition(((cst_transition *)(*r)[i])->x,
                             vector<string>(((cst_transition *)(*r)[i])->x.size(), s1));
      t->sgd = (*r)[i]->sgd;
      i--;
    }
    else if (j>jj && (*dr)[j]->kind() == SPL_TR)  {
      string s0 = "0";
      if (i>ii && (*r)[i]->kind() == CST_TR) {
        s0 = ((cst_transition *)(*r)[i])->x[0];
        i--;
      }
      t = new drspl_transition(((spl_transition *)(*dr)[j])->h, ((spl_transition *)(*dr)[j])->bun, s0);
      j--;
    }
    else {
      string s0 = "0", s1 = "0";
      if (i>ii && (*r)[i]->kind() == CST_TR) {
        s0 = ((cst_transition *)(*r)[i])->x[0];
        i--;
      }
      if (j>jj && (*dr)[j]->kind() == CST_TR) {
        s1 = ((cst_transition *)(*dr)[j])->x[0];
        j--;
      }
      t = new pos_transition(s0, s1);
    }
    
    automaton *b = new automaton(t);
    a->catenate(b);
    b;
  }
}

/* Given a list of phones v and category names s0 and s1, return the list in which
   each of the phones in v is mapped to that phone in s1 corresponding to
   v in s0.  The group number is used only for error reporting.  */
vector<string> *corresponding_phoneset(vector<string> *v, string s0, string s1, int group) {
  vector<string> *w;
  
  if (s0[0] == '\0') {
    fprintf(stderr, "%s:%d: group %d has no category\n", filename, line, group);
    exit(1);
  }
  /* The thing that's conditioning the split had better be cst, and moreover
     have a non-complemented set to split on. */
  if(s0[0] == '^') {
    fprintf(stderr, "%s:%d: group %d has a complemented category \"%s\"\n",
            filename, line, group, s0.c_str());
    exit(1);
  }
  if (s1[0]) {
    check_category(s1);
    /* It's a strong assumption that s0 is a valid category.
       I hope I haven't overlooked too many flaws in this assumption.  */
    if (category[s0]->size() != category[s1]->size()) {
      fprintf(stderr, "%s:%d: in group %d, categories \"%s\" and \"%s\" don't correspond\n",
              filename, line, group, s0.c_str(), s1.c_str());
      exit(1);
    }

    w = new vector<string>(v->size());
    for(int k = v->size()-1; k>=0; k--) {
      for(int l = category[s0]->size()-1; l>=0; l--)
        if((*category[s0])[l] == (*v)[k]) {
          (*w)[k] = (*category[s1])[l];
          break;
        }
    }
  }
  else
    w = new vector<string>(*v);

  return w;
}

/* Expand the remaining split-groups in a full (non-determinized) automaton.  */
automaton *split(automaton *a) {
  vector<int> groups;
  vector<bool> ref_firsts;
  vector<set<int> *> splittends;
  vector<int> sizes;
  vector<vector<string> *> ins;
  vector<vector<string> *> outs;
  int n = a->q.size();
  
  /* Find all active split-groups, by looking for referencing transitions, i.e.
     those of type spl or following types.  */
  for(int i=n-1; i>=0; i--)
    for(int j=a->q[i].t.size()-1; j>=0; j--) 
      if (a->q[i].t[j]->kind() >= SPL_TR) {
        //printf("considering state %d transition %d\n", i, j);
        int group = ((splitting_transition *)a->q[i].t[j])->bun;
        //printf("group is %d\n", group);
        /* The same transition both defining and referencing a group
           is just scary.  */
        if (a->q[i].t[j]->sgd != -1) {
          fprintf(stderr, "%s:%d: labelled category of group %d references group %d\n",
                  filename, line, a->q[i].t[j]->sgd, group);
          exit(1);
        }
        /* We don't allow multiple references to the same group.  */
        if (find(groups.begin(), groups.end(), group) != groups.end()) {
          fprintf(stderr, "%s:%d: multiple references to group %d\n", filename, line, group);
          exit(1);
        }
        
        /* Find the matching definition.  If there's none or multiple such,
           we're in trouble.  */
        bool found_one = false;
        int i1, j1;
        for(int ii=a->q.size()-1; ii>=0; ii--)
          for(int jj=a->q[ii].t.size()-1; jj>=0; jj--)
            if (a->q[ii].t[jj]->sgd == group) {
              //printf("found a definition, state %d transition %d\n", ii, jj);
              if (found_one) {
                fprintf(stderr, "%s:%d: multiple labels for group %d\n", filename, line, group);
                exit(1);          
              }
              else {
                i1 = ii; j1 = jj; found_one = true;
              }
            }
         if (!found_one) {
          fprintf(stderr, "%s:%d: no label for group %d\n", filename, line, group);
          exit(1);          
        }
 
        /* At this point, transition j from state i references transition j1
           from state i1.  Check that they actually delimit a group of
           states, i.e. that the only boundaries changing between nodes
           to be split and nodes not to be split are our transitions.  */
        set<int> *s = new set<int>();
        set<int> *ss = new set<int>();
        bool ref_first; // is the referencing edge first?
         reachable_excluding(a, s, a->q[i].t[j]->d, i1, j1);
         reachable_excluding(a, ss, a->q0, i, j);
         if ((s->find(i) == s->end() && s->find(i1) != s->end() && s->find(a->q1) == s->end()) &&
            (ss->find(a->q[i1].t[j1]->d) == ss->end())) {
          ref_first = true;
        }
        else {
          s->clear(); ss->clear();
           reachable_excluding(a, s, a->q[i1].t[j1]->d, i, j);
           reachable_excluding(a, ss, a->q0, i1, j1);
           if ((s->find(i1) == s->end() && s->find(i) != s->end() && s->find(a->q1) == s->end()) &&
              (ss->find(a->q[i].t[j]->d) == ss->end())) {
            ref_first = false;
          }
          else {
            fprintf(stderr, "%s:%d: group %d couldn't be joined\n", filename, line, group);
            exit(1);                      
          }
        }
        //printf("ref_first is %d\n", ref_first);

        string s0 = split_category[group];
        string s1 = ((splitting_transition *)a->q[i].t[j])->h;
        vector<string> *v = new vector<string>(((cst_transition *)a->q[i1].t[j1])->x);
        vector<string> *w = corresponding_phoneset(v, s0, s1, group);
        
        /* Having checked all the conditions above, we may do this.  */
        groups.push_back(group);
        ref_firsts.push_back(ref_first);
        splittends.push_back(s);
        sizes.push_back(v->size());
        if (ref_first) {
          ins.push_back(w);
          outs.push_back(v);
        }
        else {
          ins.push_back(v);
          outs.push_back(w);
        }
      }

  /* Now that we've enumerated the splits to be made, determine the number
     of states in the split version.  The numbering of states in this
     product is big-endian, i.e. the fragment of group 0 varies
     most slowly.  */
  vector<int> multiplicity(n, 1), psum(n, 0);
  int sum = 0;
  //printf("multiplicities are");
  for(int i=0; i<n; i++) {
    for(int j=groups.size()-1; j>=0; j--)
      if(splittends[j]->find(i) != splittends[j]->end())
        multiplicity[i] *= sizes[j];
    psum[i] = sum;
    sum += multiplicity[i];
    //printf(" %d", multiplicity[i]);
  }
  //printf(", sum is %d\n", sum);
  
  automaton *b = new automaton(sum);
  b->q0 = psum[a->q0];
  b->q1 = psum[a->q1];
  
  /* For each state, create the transitions from all of its fragments.
     Each transition is copied to run between each of these fragments and
     the corresponding fragment of the destination.  */
  for(int i=n-1; i>=0; i--)
    for(int j=a->q[i].t.size()-1; j>=0; j--) {
      int d = a->q[i].t[j]->d; // the destination
      int group_in = -1, group_out = -1, pprod = 1, sprod, uprod;

      for(int k=groups.size()-1; k>=0; k--) {
        bool i_found = splittends[k]->find(i) != splittends[k]->end();
        bool d_found = splittends[k]->find(d) != splittends[k]->end();
        if((i_found?1:0) ^ (d_found?1:0)) {
          if (group_in != -1 || group_out != -1) {
            fprintf(stderr, "%s:%d: this shouldn't happen!  multiple split-effects on %d->%d\n",
                    filename, line, i, d);
            exit(1);                      
          }
          if (i_found)
            group_out = k;
          else
            group_in = k;
          sprod = pprod;
          uprod = sizes[k];
        }
        if (i_found)
          pprod *= sizes[k];
      }
      //printf("in and out splits for %d->%d are %d %d, sprod is %d, uprod is %d\n",
      //       i, d, group_in, group_out, sprod, uprod);

      for(int k=multiplicity[i]-1; k>=0; k--) {
        if (group_in != -1) {
          for(int l=uprod-1; l>=0; l--) {
            transition *t;
            if (ref_firsts[group_in])
              t = ((splitting_transition *)a->q[i].t[j])->select((*ins[group_in])[l]);
            else
              t = a->q[i].t[j]->forselect((*ins[group_in])[l]);
            t->d = psum[d] + k%sprod + sprod*uprod*(k/sprod) + sprod*l;
            b->q[psum[i] + k].t.push_back(t);
          }
        }
        else if (group_out != -1) {
          transition *t;
          if (ref_firsts[group_out])
            t = a->q[i].t[j]->forselect((*outs[group_out])[(k/sprod)%uprod]);
          else
            t = ((splitting_transition *)a->q[i].t[j])->select((*outs[group_out])[(k/sprod)%uprod]);
          t->d = psum[d] + k%sprod + sprod*(k/(sprod*uprod));
          b->q[psum[i] + k].t.push_back(t);
        }
        else {
          transition *t;
          /* I'm slightly worried about the depth of the copying here.  */
          switch (a->q[i].t[j]->kind()) {
            case POS_TR:
              t = new pos_transition(*(pos_transition *)a->q[i].t[j]); break;
            case CST_TR:
              t = new cst_transition(*(cst_transition *)a->q[i].t[j]); break;
            case NEG_TR:
              t = new neg_transition(*(neg_transition *)a->q[i].t[j]); break;
            case NER_TR:
              t = new ner_transition(*(ner_transition *)a->q[i].t[j]); break;
          }
          t->d = psum[d] + k;
          b->q[psum[i] + k].t.push_back(t);
        }
      }
    }

  for(int i=groups.size()-1; i>=0; i--) {
    splittends[i];
    ins[i];
    outs[i];
  }
      
  return b; 
}

/* Do a search to determine which states are reachable from x,
   while avoiding the z\/th transition from y.  Save them in s.  This is
   necessary to determine whether splits can be done and which vertices
   they affect.

   We use breadth-first search (this isn't really crucial). */
void reachable_excluding(automaton *a, set<int> *s, int x, int y, int z) { 
  /* We may already have gotten here.  */
  if (s->find(x) != s->end())
    return;
  s->insert(x);
  for(int j=a->q[x].t.size()-1; j>=0; j--) {
    if (x == y && j == z)
      continue; // avoid following this one!
    reachable_excluding(a, s, a->q[x].t[j]->d, y, z);    
  }
}
                         
//int main() {
//  yyparse();
//  printf("\n");
//  return 0;
//}

