#include "apply.h"
#include <string.h>

/* This is in lieu of fgetln, which apparently isn't portable.  */
char *line_buffer = NULL;
int line_buffer_size = 256;

char *read_arbitrary_length_line(FILE *stream, size_t *p_len) {
  if (!line_buffer) {
    line_buffer = (char *)malloc(line_buffer_size * sizeof(char));
  }
  
  line_buffer[0] = '\0';
  
  while (1) {
    line_buffer[line_buffer_size-1] = -1;
    if(NULL == fgets(line_buffer + strlen(line_buffer), line_buffer_size - strlen(line_buffer), stream)) {
      return NULL;
    }
    if(line_buffer[line_buffer_size-1] == '\0' && line_buffer[line_buffer_size-2] != '\n') {
      char *d = line_buffer;
      line_buffer_size *= 2;
      line_buffer = (char *)malloc(line_buffer_size * sizeof(char));
      strcpy(line_buffer, d);
      free(d);
    }
    else
      break;
  }

  *p_len = strlen(line_buffer);
  return line_buffer;
}

/* Tokenise according to the modifier character types from the lexer
   (although using an algorithm that's somewhat more simplistic, and
   that won't necessarily match up for odd input).
   Also append the initial and final "#".  */
vector<string> *tokenise(string s) {
  vector<string> *x = new vector<string>(0);
  bool append_next = false;
  int gather = 0;
  for(int i=s.size()-1; i>=0; i--) {
    if (append_next)
      x->back() = s[i] + x->back();
    else
      x->push_back(string(1, s[i]));
    switch (modtype[s[i]]) {
      case MOD01:
        append_next = false; gather = 1; break;
      case MOD10:
        append_next = true; gather = 0; break;
      case MOD02:
        append_next = false; gather = 2; break;
      case MOD11:
        append_next = true; gather = 1; break;
      default: /* catches CPHONE */
        append_next = false; gather = 0; break;
    }
    if (x->size() <= gather)
      return NULL;
    for(int k=gather-1; k>=0; k--) {
      string t = x->back();
      x->pop_back();
      x->back() = t + x->back();
    }
  }
  if (append_next)
    return NULL;

  x->push_back("#");
  reverse(x->begin(), x->end());
  x->push_back("#");
  return x;
}

void apply_changes() {
  char *p;
  size_t p_len;
  
  while (p = read_arbitrary_length_line(stdin, &p_len)) {
    set<vector<string> > s0, s1, *s_old = &s0, *s_new = &s1, *s_tmp;
    vector<string> *x; 

    // strip off the final newline; if the line is then empty, don't do anything
    p[p_len-1] = '\0';
    for (; *p == ' ' || *p == '\t'; p++, p_len--);
    if (first_word_only)
      p = strsep(&p, " \t");
    if(p[0] == '\0') 
      continue;
    
    x = tokenise(string(p));
    if (x == NULL) {
      fprintf(stderr, "couldn't tokenise input word \"%s\"\n", p);
      continue;
    }
    s_old->insert(*x);
    delete x;

    for(int i=0; i<changes.size(); i++) {
      for(set<vector<string> >::iterator ii=s_old->begin(); ii!=s_old->end(); ++ii) {
        vector<string> v = *ii;
        s_tmp = changes[i]->transduce(&v, change_stuff[i]->max_epen, change_stuff[i]->reflect); 

        /* Try to fix outcomes which aren't bounded by "#"s.  If we can't, remove them.
           This bit of a hack is necessitated by the unfortunate choice of "#" as both
           word boundaries, and I hope it doesn't cause problems elsewhere.  */
        if(!s_tmp->empty()) {
          for(set<vector<string> >::iterator jj=s_tmp->begin(), ii=jj++; ii!=s_tmp->end(); ii=jj++) {
            if(ii->size() < 1 || (*ii)[0] != "#" || (*ii)[ii->size()-1] != "#") {
              vector<string> fix = *ii;
              vector<string>::iterator kk = fix.begin();
              s_tmp->erase(ii);
              
              for(; kk != fix.end() && *kk != "#"; ++kk);
              if (kk == fix.end()) 
                continue;
              fix.erase(fix.begin(), kk);
              kk = fix.begin();
              for(++kk; kk != fix.end() && *kk != "#"; ++kk);
              if (kk == fix.end()) 
                continue;
              fix.erase(++kk, fix.end());
              s_tmp->insert(fix); // it may be reexamined, but that's no big deal
            }
            if(s_tmp->empty())
              break;
          }
        }

        if (debug_changes && (s_tmp->size() != 1 || *s_tmp->begin() != *ii)) {
          if (reverse_changes)
            printf("%s yields \"", change_stuff[i]->name.c_str());
          else
            printf("%s applies to \"", change_stuff[i]->name.c_str());            
          for(int k=1; k<ii->size()-1; k++)
            printf("%s", (*ii)[k].c_str());
          if (reverse_changes)
            printf("\" when applied to");  
          else
            printf("\", yielding");
          for(set<vector<string> >::iterator ii=s_tmp->begin(); ii!=s_tmp->end(); ++ii) {
            printf(" \"");
            for(int k=1; k<ii->size()-1; k++)
              printf("%s", (*ii)[k].c_str());
            printf("\"");
          }
          printf("\n");
        }

        /* Complain if there's no words as output; this was hopefully due
           to a constraint failure.  */
        if (complaint && s_tmp->empty()) {
          fprintf(stderr, "warning: \"");
          for(int k=1; k<ii->size()-1; k++)
            fprintf(stderr, "%s", (*ii)[k].c_str());
          fprintf(stderr, "\" doesn't satisfy constraint %s\n", change_stuff[i]->name.c_str()); 
        }
        
        s_new->insert(s_tmp->begin(), s_tmp->end());
        delete s_tmp;
      }

      s_old->clear();
      s_tmp = s_old; s_old = s_new; s_new = s_tmp;
    }

    /* Output all the possibilities, one per line.  */
    if (display_wedges) {
      if (reverse_changes)
        printf("%s < ", p);
      else
        printf("%s > ", p);
    }
    for(set<vector<string> >::iterator ii=s_old->begin(); ii!=s_old->end(); ++ii) {
      if (ii!=s_old->begin())
        printf(" ");
      for(int k=1; k<ii->size()-1; k++)
        printf("%s", (*ii)[k].c_str());
    }
    if (display_brackets)
      printf(" [%s]", p);
    printf("\n");
  }
}



bool handle_args(int argc, char **argv) {
  if (argc <= 1)
    return true;
  
  for(int i=1; i<argc; i++) {
    /* Here we'll eventually handle command line options.  Some to implement are:
       - output options like MCR's and input options like Geoff's (how about charset?)
    */
    if (!strcmp(argv[i], "-d")) // print intermediate change results
      debug_changes = true;
    else if (!strcmp(argv[i], "-D")) // print the automata generated for each sound change
      debug_automata = true;
    else if (!strcmp(argv[i], "-q")) // don't complain when a constraint fails
      complaint = false;
    else if (!strcmp(argv[i], "-r")) { // go in reverse; implies no complaint
      reverse_changes = true; 
      complaint = false;
    }
    else if (!strcmp(argv[i], "-i")) { // use an input file
      if (i >= argc-1)
        return true;
      freopen(argv[++i], "r", stdin);
    }
    else if (!strcmp(argv[i], "-o")) { // use an output file
      if (i >= argc-1)
        return true;
      freopen(argv[++i], "w", stdout);
    }
    else if (!strcmp(argv[i], "-b")) // display with brackets
      display_brackets = true;
    else if (!strcmp(argv[i], "-B")) // display with wedges
      display_wedges = true;
    else if (!strcmp(argv[i], "-f")) // only convert the first word on each line
      first_word_only = true;
    else {
      if (filename != NULL)
        return true;
      filename = strdup(argv[i]);
    }
  }

  return (filename == NULL);
}

int main(int argc, char **argv) {
  if (handle_args(argc, argv)) {
    fprintf(stderr, "usage: %s [options] <sound change file>\n", argv[0]);
    fprintf(stderr, "allowed options are\n");
    fprintf(stderr, "-r          apply sound changes in reverse\n");
    fprintf(stderr, "-d          print intermediate sound change results\n");
    fprintf(stderr, "-D          print generated transducers\n");
    fprintf(stderr, "-q          don't complain when a constraint fails\n");
    fprintf(stderr, "-i <file>   read input words from file\n");
    fprintf(stderr, "-o <file>   write output words to file\n");
    fprintf(stderr, "-b          repeat the input word in brackets []\n");
    fprintf(stderr, "-B          repeat the input word with a wedge < >\n");
    fprintf(stderr, "-f          only process the first word on each line\n");
    exit(1);
  }

  if (NULL == (yyin = fopen(filename, "r"))) {
    fprintf(stderr, "couldn't open \"%s\"\n", filename);
    exit(1);
  }
  
  yyparse();

  apply_changes();
  
  return 0;
}

