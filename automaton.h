#ifndef __RSCA_AUTOMATON
#define __RSCA_AUTOMATON

#include <stdio.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <algorithm>

#include "forfc.h"

using namespace std;



/* The split-group resolver exploits the fact that all of the split-referencing
   transitions come at the end, with SPL_TR first.  */
enum {POS_TR = 0, CST_TR, NEG_TR, NER_TR, SPL_TR, RSPL_TR, DRSPL_TR};

struct automaton_state;

struct transition {
  int d; // destination state
  int sgd; // which split-group this transition defines, -1 for none

  // yes, I know there is C++ support for dynamic polymorphism.  
  virtual int kind() = 0;
  virtual void display() = 0;
  /* either which strings trigger this transition or its complement, whichever
     is finite */
  virtual forfc<string> trigger_set() = 0;
  /* the next two are only correct if t is known to be a trigger */
  virtual string outcome(string t) = 0;
  virtual vector<string> all_outcomes(string t) = 0;
  virtual transition *forselect(string &t) = 0;

  virtual ~transition() {}
};

struct pos_transition : transition {
  vector<string> x;
  vector<string> y;

  pos_transition(vector<string> x_, vector<string> y_) : x(x_), y(y_) { sgd = -1; }
  pos_transition(string x0, string y0) {
    x = vector<string>(1);
    y = vector<string>(1);
    x[0] = x0;
    y[0] = y0;
    sgd = -1;
  }

  int kind() { return POS_TR; }
  void display() {
    if (sgd != -1)
      printf("[%d] ", sgd);
    for(int i = x.size()-1; i >= 0; i--)
      printf("%s/%s ", x[i].c_str(), y[i].c_str());
    printf("-> %d", d);
  }
  forfc<string> trigger_set() { return forfc<string>(x, true); }
  string outcome(string t) {
    for(int i=x.size()-1; i>=0; i--)
      if(x[i]==t)
        return y[i];
    return ""; // an invalid case
  }
  vector<string> all_outcomes(string t) {
    vector<string> v;
    for(int i=0; i<x.size(); i++)
      if(x[i]==t)
        v.push_back(y[i]);
    return v;
  }
  transition *forselect(string &t) { return new pos_transition(t, outcome(t)); }
    
  ~pos_transition() {}
};

struct cst_transition : transition {
  vector<string> x;

  cst_transition(vector<string> x_) : x(x_) { sgd = -1; }
  cst_transition(string x0) {
    x = vector<string>(1);
    x[0] = x0;
    sgd = -1; 
  }

  int kind() { return CST_TR; }
  void display() {
    if (sgd != -1)
      printf("[%d] ", sgd);
    for(int i = x.size()-1; i >= 0; i--)
      printf("%s ", x[i].c_str());
    printf("-> %d", d);
  }
  forfc<string> trigger_set() { return forfc<string>(x, true); }
  string outcome(string t) { return t; }
  vector<string> all_outcomes(string t) { return vector<string>(1, t); }
  transition *forselect(string &t) { return new cst_transition(t); }

  ~cst_transition() {}
};

struct neg_transition : transition {
  vector<string> z;

  neg_transition(vector<string> z_) : z(z_) { sgd = -1; }

  int kind() { return NEG_TR; }
  void display() {
    if (sgd != -1)
      printf("[%d] ", sgd);
    for(int i = z.size()-1; i >= 0; i--)
      printf("^%s ", z[i].c_str());
    printf("-> %d", d);
    sgd = -1;
  }
  forfc<string> trigger_set() { return forfc<string>(z, false); }
  string outcome(string t) { return t; }
  vector<string> all_outcomes(string t) { return vector<string>(1, t); }
  transition *forselect(string &t) { return new cst_transition(t); }

  ~neg_transition() {}
};

struct ner_transition : transition {
  vector<string> z;
  string s;

  ner_transition(vector<string> z_, string s_ = "0") : z(z_), s(s_) { sgd = -1; }

  int kind() { return NER_TR; }
  void display() {
    if (sgd != -1)
      printf("[%d] ", sgd);
    for(int i = z.size()-1; i >= 0; i--)
      printf("^%s/%s ", z[i].c_str(), s.c_str());
    printf("-> %d", d);
    sgd = -1;
  }
  forfc<string> trigger_set() { return forfc<string>(z, false); }
  string outcome(string t) { return s; }
  vector<string> all_outcomes(string t) { return vector<string>(1, s); }
  transition *forselect(string &t) { return new pos_transition(t, s); }

  ~ner_transition() {}
};

struct splitting_transition : transition {
  string h;
  int bun; // which transition the split of this one is conditioned to

  /* we don't need these */
  forfc<string> trigger_set() { return forfc<string>(vector<string>(), true); }
  string outcome(string t) { return ""; }
  vector<string> all_outcomes(string t) { return vector<string>(); }
  transition *forselect(string &t) { return select(t); } // I dunno about this one.
  
  /* reduce this to the most similar kind of non-split transition, for phone s */
  virtual transition *select(string &s) = 0; 
  virtual ~splitting_transition() {}
};

struct spl_transition : splitting_transition {
  spl_transition(string h_, int bun_) { h = h_; bun = bun_; sgd = -1; }

  int kind() { return SPL_TR; }
  void display() {
    if (sgd != -1)
      printf("[%d] ", sgd);
    printf("#%d \"%s\" -> %d", bun, h.c_str(), d);
  }
  transition *select(string &s) { return new cst_transition(s); }
  
  ~spl_transition() {}
};

struct rspl_transition : splitting_transition {
  string e;
  rspl_transition(string h_, int bun_, string e_ = "0") : e(e_) { h = h_; bun = bun_; sgd = -1; }

  int kind() { return RSPL_TR; }
  void display() {
    if (sgd != -1)
      printf("[%d] ", sgd);
    printf("#%d \"%s\"/%s -> %d", bun, h.c_str(), e.c_str(), d);
  }
  transition *select(string &s) { return new pos_transition(s, e); }

  ~rspl_transition() {}
};

struct drspl_transition : splitting_transition {
  string e;
  drspl_transition(string h_, int bun_, string e_ = "0") : e(e_) { h = h_; bun = bun_; sgd = -1; }

  int kind() { return DRSPL_TR; }
  void display() {
    printf("#%d %s/\"%s\" -> %d", bun, e.c_str(), h.c_str(), d);
  }
  transition *select(string &s) { return new pos_transition(e, s); }

  ~drspl_transition() {}
};





/* A partial application of an automaton to a string.  */
struct application {
  vector<string> y;
  int q;

  application(vector<string> y_, int q_) : y(y_), q(q_) {}
  bool operator<(const application &a) const {
    if (y != a.y)
      return y < a.y;
    return q < a.q;
  }
  void display() const {
    printf("[%d]", q);
    for(int i=0; i<y.size(); i++)
      printf(" %s", y[i].c_str());
  }
};



struct automaton_state {
  bool accept; // accepting?
  vector<transition *> t; // transitions out

  automaton_state() {
    accept = false;
    t = vector<transition *>(0);
  }

  ~automaton_state() {
  }
};


struct automaton {
  int q0, q1; // start and end states
  int mq0, mq1; // if another automaton was merged in, its start and end states
  vector<automaton_state> q; // states
  
  automaton(int n = 0);
  automaton(transition *t);
  automaton(char *x, char *y = NULL);
  automaton(vector<string> *cat, bool p, int group = -1);
  automaton(vector<string> *cat0, vector<string> *cat1);

  void free_transitions();
  
  void merge(automaton *a);
  void renumber(int r, int s);
  void unify_states(int r, int s);
  void delete_state(int r);

  void catenate(automaton *a);
  void kleene_star();
  void alternate(automaton *a);
  void optionalize();
  void kleene_plus();

  void reflect();
  bool invert();

  void display();
  
  void zero_close(set<pair<int, vector<string> > > *s, int k, vector<string> &output,
                  bool catch_form1, int n = -1);
  int get_or_create_determination(set<int> &t0, map<set<int>, int> &label,
                                  int &m, deque<set<int> > &queue, int n);
  int zero_reach(set<pair<int,vector<string> > > &g, vector<set<pair<int, vector<string> > > > &zero_closure,
                 bool not_sporadic, map<set<int>, int> &label, int &m, deque<set<int> > &queue, int aq1, int n);
  automaton *determinise(bool not_sporadic = true, int initial_form = 0, bool respecting_conflicts = true);

  void apply_zeros(const application *a, set<application> *s, vector<int> &c, int max_epen);
  set<vector<string> > *transduce(vector<string> *x, int max_epen = 1, bool reflect = false);
};

#endif
