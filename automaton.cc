#include "automaton.h"

/* Construct an empty automaton with n states.  n defaults to 0.  */
automaton::automaton(int n) {
  q = vector<automaton_state>(n);
  q0 = q1 = 0;
}

/* Construct an automaton to contain a given transition. */
automaton::automaton(transition *t) {
  q = vector<automaton_state>(2);
  q0 = 0;
  q1 = 1;

  t->d = q1;
  q[q0].t.push_back(t);
}

/* Construct an automaton for a single phone.  */
automaton::automaton(char *x, char *y) {
  q = vector<automaton_state>(2);
  q0 = 0;
  q1 = 1;

  if (y == NULL) {
    string sx(x);
    cst_transition *t = new cst_transition(sx);
    t->d = q1;
    q[q0].t.push_back(t);
  }
  else {
    string sx(x), sy(y);
    pos_transition *t = new pos_transition(sx, sy);
    t->d = q1;
    q[q0].t.push_back(t);
  }
}

/* Construct an automaton for a list of phones.  The second argument
   is whether to transition on just these phones (true) or
   all but them (false).  The third is a group number to
   let the transition define.  */
automaton::automaton(vector<string> *cat, bool p, int group) {
  q = vector<automaton_state>(2);
  q0 = 0;
  q1 = 1;

  if (p) {
    cst_transition *t = new cst_transition(*cat);
    t->d = q1;
    t->sgd = group;
    q[q0].t.push_back(t);
  }
  else {
    neg_transition *t = new neg_transition(*cat);
    t->d = q1;
    t->sgd = group;
    q[q0].t.push_back(t);
  }
}

/* Construct an automaton for two corresponding lists of phones.  */
automaton::automaton(vector<string> *cat0, vector<string> *cat1) {
  q = vector<automaton_state>(2);
  q0 = 0;
  q1 = 1;

  pos_transition *t = new pos_transition(*cat0, *cat1);
  t->d = q1;
  q[q0].t.push_back(t);
}

/* Free the transitions in an automaton.  This is not in the destructor
   because it's not always appropriate; it should be called just when
   the transitions haven't been used in a merge.  */
void automaton::free_transitions() {
  for(int i = q.size() - 1; i >= 0; i--)
    for(int j = q[i].t.size()-1; j >= 0; j--)
      delete q[i].t[j];
}

/* Add the states of the automaton a to this automaton, remembering
   the start and end states of a.  */
void automaton::merge(automaton *a) {
  int n = q.size(); // our number of states

  /* Record the new numbers of the start and end states.  */
  mq0 = a->q0 + n;
  mq1 = a->q1 + n;

  /* Add the new states.  This probably doesn't copy deeply yet (TODO). */
  q.insert(q.end(), a->q.begin(), a->q.end());

  /* Renumber them to point to themselves.  */
  for (int i = n + a->q.size() - 1; i >= n; i--)
    for (int j = q[i].t.size() - 1; j >= 0; j--)
      q[i].t[j]->d += n;
}

/* Merge state s into state r, by changing all references to state s
   to refer to state r.  Do not adjust the size of the vector of states.  */
void automaton::renumber(int r, int s) {
  /* Change special states.  */
  if (q0 == s) q0 = r;
  if (q1 == s) q1 = r;
  if (mq0 == s) mq0 = r;
  if (mq1 == s) mq1 = r;

  /* Put the transitions currently in state s in state r instead.  */
  q[r].t.insert(q[r].t.end(), q[s].t.begin(), q[s].t.end());
  q[s].t.clear();

  /* Change all destinations of transitions that are s to r.  */
  for (int i = q.size() - 1; i >= 0; i--)
    for (int j = q[i].t.size() - 1; j >= 0; j--)
      if (q[i].t[j]->d == s)
        q[i].t[j]->d = r;

  q[r].accept = q[s].accept;
}

/* Merge state s into state r, by changing all references to state s
   to refer to state r.  Then renumber the states to use consecutive
   numbers, dropping one state. */
void automaton::unify_states(int r, int s) {
  if (r == s) return;
  renumber(r, s);
  if (s < q.size() - 1)
    renumber(s, q.size() - 1);
  q.pop_back();

  /* We don't care about acceptance of states: in the automata that
     we do this to we haven't yet defined accepting states.  */
}

/* Delete state r.  This involves first deleting all transitions to
   and from r, and then doing an appropriate renumbering.
   If r is either q0 or q1, it'll be left as it was; beware!  */
void automaton::delete_state(int r) {
  /* Remove all transitions whose destination is r.  */
  for (int i = q.size() - 1; i >= 0; i--)
    for (int j = q[i].t.size() - 1; j >= 0; j--)
      if (q[i].t[j]->d == r) {
        delete q[i].t[j]; // thus we assume this transition isn't pointed to elsewhere
        q[i].t[j] = q[i].t[q[i].t.size()-1];
        q[i].t.pop_back();
      }

  /* Remove the ones whose origin is r.  */
  q[r].t.clear();

  /* Move the last state to fill the void.  */
  if (r < q.size() - 1)
    renumber(r, q.size() - 1);
  q.pop_back();  
}

/* Become the catenation (this a).  */
void automaton::catenate(automaton *a) {
  merge(a);
  unify_states(q1, mq0);
  q1 = mq1;
}

/* Become the Kleene closure (this*).  */
void automaton::kleene_star() {
  unify_states(q0, q1);
  cst_transition *u = new cst_transition("0");
  u->d = q.size();
  q[q1].t.push_back(u);
  q1 = q.size();
  q.resize(q1 + 1);
}

/* Become the alternation (this | a).  */
void automaton::alternate(automaton *a) {
  merge(a);
  unify_states(q1, mq1);
  cst_transition *t = new cst_transition("0");
  t->d = q0;
  cst_transition *u = new cst_transition("0");
  u->d = mq0;
  q0 = q.size();
  q.resize(q0 + 1);
  q[q0].t.push_back(t);
  q[q0].t.push_back(u);
}

/* Become the optionalization (this?).   Note that in this and
   the next function 0 represents the empty string.  */
void automaton::optionalize() {
  cst_transition *t = new cst_transition("0");
  t->d = q1;
  q[q0].t.push_back(t);
}

/* Become the nonempty Kleene closure (this+).  */
void automaton::kleene_plus() {
  cst_transition *t = new cst_transition("0");
  t->d = q0;
  q[q1].t.push_back(t);
  cst_transition *u = new cst_transition("0");
  u->d = q.size();
  q[q1].t.push_back(u);
  q1 = q.size();
  q.resize(q1 + 1);
}



/* Become the left-to-right reflection of this automaton.
   Note that the automaton produces _does not respect_ our regular operator
   conventions, that automata may return to their first state and are
   not permitted to leave their last state by a transition.  */
void automaton::reflect() {
  int tmp;
  tmp = q0; q0 = q1; q1 = tmp;
  tmp = mq0; mq0 = mq1; mq1 = tmp;

  vector<automaton_state> q_(q.size());

  for(int i=q.size()-1; i>=0; i--) {
    q_[i].accept = q[i].accept;
    for (int j=q[i].t.size()-1; j>=0; j--) {
      transition *t = q[i].t[j];
      int d = t->d;
      t->d = i;
      q_[d].t.push_back(t);
    }
  }

  q = q_;
}

/* Turn this transducer into one that computes the inverse of the function
   it currently does.  This is done simply by swapping input and output
   of all transitions.
   The return value is whether we succeed: some sound changes are irreversible,
   principally those which map an infinite set to a single phone (since the
   inverse of such a change would create infinite sets of possibilities).  */
bool automaton::invert() {
  for (int i = q.size() - 1; i >= 0; i--)
    for (int j = q[i].t.size() - 1; j >= 0; j--) {
      switch (q[i].t[j]->kind()) {
        case CST_TR: case NEG_TR:
          break;
        case POS_TR:
          ((pos_transition *)q[i].t[j])->x.swap(((pos_transition *)q[i].t[j])->y);
          break;
        case NER_TR:
          return false;
      }
    }
      return true;
}



/* Display this automaton.  This is essentially for testing.  */
void automaton::display() {
  int n = q.size();
  printf("%d states, start %d, end %d\n", n, q0, q1);
  for(int i = 0; i < n; i++) {
    if (q[i].accept)
      printf("(%d) ",i);
    else
      printf(" %d  ",i);
    for(int j = q[i].t.size() - 1; j >= 0; j--) {
      q[i].t[j]->display();
      if (j) printf(", ");
    }
    printf("\n");
  }
}



/* Find all states that can be reached from this one by following
   only zero-transitions (those triggered by zero, whether or not
   they're triggered by other things; but not transitions triggered
   by infinite sets).

   State numbering is for compatibility with the meanings assigned
   to forms and state numbers in determinise().  
   There's a catch in dealing with states of form 0, since we
   may encounter the first rewriting here.  Give these states
   an otherwise invalid form 3; we'll handle references to this
   form in the state retrieval.

   There's an analogous catch even in dealing with states of
   form 1, so give these form 4; but there's another catch, in
   that we also need to record where these states actually go.
   For this we have the argument catch_form1; if it's true, we
   use form 4, else not.  */
void automaton::zero_close(set<pair<int, vector<string> > > *s, int k, vector<string> &output,
                           bool catch_form1, int n) {
  /* Look for evidence that we've already dealt with this state.
     I've actually got no idea how sound this is.  */
  for(set<pair<int, vector<string> > >::iterator ii=s->begin(); ii!=s->end(); ++ii)
    if (ii->first == k)
      return;
  if (n == -1) // compute the size if it wasn't given
    n = q.size();
  s->insert(pair<int, vector<string> >(k, vector<string>(output)));

  int ka = k%n, kk = k/n;
  for(int j=q[ka].t.size()-1; j>=0; j--) {
    if(q[ka].t[j]->trigger_set().contains("0")) {
      if(q[ka].t[j]->kind() == CST_TR)
        zero_close(s, q[ka].t[j]->d + kk*n, output, n);
      else if(q[ka].t[j]->kind() == POS_TR) {
        if (kk != 0 && !(kk == 1 && catch_form1)) {
          vector<string> u = q[ka].t[j]->all_outcomes("0");
          vector<string> output0(output);
          for(int l=u.size()-1; l>=0; l--) {
            if(u[l] != "0" && kk == 1) 
              output0.push_back(u[l]); 
            zero_close(s, q[ka].t[j]->d + kk*n, output0, n);
          }
        }
        else if (kk == 1) {
          /* These also deserve special treatment, just as the form 0s do.
             But there's a catch!  */
          s->insert(pair<int, vector<string> >(ka + 4*n, vector<string>(0)));
        }
        else {
          /* This is the first encounter of a rewriting transition in form 0, so
             the output had better be empty. */
          s->insert(pair<int, vector<string> >(ka + 3*n, vector<string>(0)));
        }
      }
    }
  }
}

/* Given a set of states from the determinization construction, return
   a state number for it.  If we've already made it, simply look up
   and return its state number.  If not, make it and do the appropriate thing.  */
int automaton::get_or_create_determination(set<int> &t0, map<set<int>, int> &label,
                                           int &m, deque<set<int> > &queue, int n) {
  if(label.find(t0) == label.end()) {
    /* We've encountered a new state.  */
    label[t0] = m;
    queue.push_back(t0);

    q.resize(m+1);
    /* Statesets with a state of form 1 are not accepting. */
    q[m].accept = true;
    for(set<int>::iterator kk=t0.begin(); kk!=t0.end(); ++kk)
      if(*kk/n == 1) {
        q[m].accept = false;
        break;
      }

    m++;
  }
  return label[t0];
}

/* Given a zero closure-style set of states and strings, return a state that
   can be used to reference it.  If there are no nonempty strings, and every
   state in the set is decent (i.e. not in the fake forms 3 or 4), this
   will be the actual state for that set.  Otherwise, we'll construct
   a new state (and not remember it, so we'll reconstruct it if it comes to that)
   which serves.   The reason for the exception is to keep the number of states down
   (not especially to minimize computation time, which is a bit of a lost cause).  */
int automaton::zero_reach(set<pair<int,vector<string> > > &g,
                          vector<set<pair<int,vector<string> > > > &zero_closure, bool not_sporadic,
                          map<set<int>, int> &label,
                          int &m, deque<set<int> > &queue, int aq1, int n) {
  /* Test for the no nonempty and decent case. */
  set<pair<int,vector<string> > >::iterator ii;
  set<int> t0;
  for(ii = g.begin(); ii != g.end(); ++ii) {
    t0.insert(ii->first);
    if(ii->second != vector<string>(0) || ii->first/n == 3 || ii->first/n == 4)
      break;
  }
  if (ii == g.end())
    return get_or_create_determination(t0, label, m, queue, n);

  /* So we're not getting off so easily: it's the case with rewriting.
     Find all relevant strings, and store form 3 and 4 states specially.  */
  map<vector<string>, set<int> > p;
  vector<int> form3;
  for(ii = g.begin(); ii != g.end(); ++ii) {
    if (ii->first/n == 3 || ii->first/n == 4)
      form3.push_back(ii->first);
    else
      p[ii->second].insert(ii->first);
  }

  /* Do a cartesian productish construction on every form 3 and 4 state.  */
  for(int i=form3.size()-1; i>=0; i--) {
    map<vector<string>, set<int> > p0;
    int k = form3[i]%n, form = form3[i]/n;
    
    /* Form 3 states come from form 0 and so transition to forms 1 and 2.
       Form 4 states come from form 1 and so transition only to form 1.

       Zero closures are nonempty, thus the structure here is a bit less general than it might be.
       It also does quite some redundant insertion.  */
    if (form == 3)
      ii = zero_closure[k+2*n].begin();
    else if (form == 4)
      ii = zero_closure[k+n].begin();
    while (ii != zero_closure[k+n].end()) {
      /* We only want _strict_ followers of this state.  */
      if (ii->first != k+n && ii->first != k+2*n)
        for(map<vector<string>, set<int> >::iterator jj=p.begin(); jj!=p.end(); ++jj) {
          vector<string> s(jj->first);
          s.insert(s.end(), ii->second.begin(), ii->second.end());
          p0[s].insert(jj->second.begin(), jj->second.end());
          p0[s].insert(ii->first);
        }

      ++ii;
      if (ii == zero_closure[k+2*n].end())
        ii = zero_closure[k+n].begin();
    }

    p0.swap(p);
  }

  /* Drop references to the final state q1 in forms 0 and 1; kill altogether
     any sets containing q1 in form 2, unless sporadic.  */
  if (!p.empty())
    for(map<vector<string>, set<int> >::iterator kk=p.begin(), jj=kk++; jj!=p.end(); jj=kk++) {
      set<int>::iterator ll = jj->second.find(aq1);
      if (ll != jj->second.end())
        jj->second.erase(ll);
      ll = jj->second.find(aq1 + n);
      if (ll != jj->second.end())
        jj->second.erase(ll);
      ll = jj->second.find(aq1 + 2*n);
      if (ll != jj->second.end()) {
        if (not_sporadic)
          p.erase(jj);
        else
          jj->second.erase(ll);
      }
    }

  /* Do the constructing.  Note that all these new states will be non-accepting,
     as is correct.  */
  int home = m++;
  q.resize(m);
  for(map<vector<string>, set<int> >::iterator jj=p.begin(); jj!=p.end(); ++jj) {
    transition *last = NULL;
    if(jj->first == vector<string>(0)) {
      last = new cst_transition("0");
      q[home].t.push_back(last);
    }
    else {
      int dest = home;
      for(int i=0; i<jj->first.size(); i++) {
        if(last != NULL)
          last->d = dest;
        last = new pos_transition("0", jj->first[i]);
        q[dest].t.push_back(last);
        if (i < jj->first.size()-1) {
          dest = m++;
          q.resize(m);
        }
      }
    }
    last->d = get_or_create_determination(jj->second, label, m, queue, n);
  }

  //printf("......  created state %d to handle\n",home);
  //for(set<pair<int,vector<string> > >::iterator kk=g.begin(); kk!=g.end(); ++kk) {
  //  printf(" %d",kk->first);
  //  for(int k=kk->second.size()-1; k>=0; k--)
  //    printf(" %s",kk->second[k].c_str());
  //  printf("\n");
  //}  
  
  return home;
}

/* Perform the modified subset construction on this transducer, to yield
   one that carries out the sound change.  Note that this does not
   actually yield a deterministic transducer, i.e. my terminology is bad.
   Also note that it has a side-effect!

   initial_form should be 0 for standard changes, 2 for constraints
   which must not be satisfied, and I suppose 1 for constraints
   that must be satisfied, though I never use this latter option.
   It can also be 0 for the determinization construction on
   true automata, without rewriting.  */
automaton *automaton::determinise(bool not_sporadic, int initial_form, bool respecting_conflicts) {
  automaton* b = new automaton(0); 
  int n = q.size();
  map<set<int>, int> label;
  int m = 0; // current state in the new automaton
  deque<set<int> > queue; // things whose transitions we need to create
  set<int> s;
  vector<set<pair<int,vector<string> > > > zero_closure(3*n); // three different forms of each state
  vector<set<pair<int,vector<string> > > > zero_closure_breaking(3*n); // ick, horrible duplication
  
  /* Add the universal transition on q0.  This is the side-effect.  */
  transition *loop = new neg_transition(vector<string>(0)); 
  loop->d = q0;
  q[q0].t.push_back(loop);

  /* For each state, find the zero closure, obtained by taking all transitions
     which are triggered by zero, including those with output.  */
  for(int i=3*n-1; i>=0; i--) {
    vector<string> closure_tmp(0);
    zero_close(&zero_closure[i], i, closure_tmp, true, n);
    zero_close(&zero_closure_breaking[i], i, closure_tmp, false, n);

    //printf("(nonbreaking) zero closure of state %d has\n",i);
    //for(set<pair<int,vector<string> > >::iterator kk=zero_closure[i].begin(); kk!=zero_closure[i].end(); ++kk) {
    //  printf(" %d",kk->first);
    //  for(int k=kk->second.size()-1; k>=0; k--)
    //    printf(" %s",kk->second[k].c_str());
    //  printf("\n");
    //}
  }

  /* Set the initial state of b to the zero reach of our initial state.  */
  b->q1 = b->q0 =
    b->zero_reach(zero_closure[q0 + n*initial_form], zero_closure_breaking, not_sporadic, label, m, queue, q1, n);

  /* This is the main loop.  */
  while(!queue.empty()) {
    s = queue.front();
    queue.pop_front();
    //printf("######  working on state %d that is the set", label[s]);
    //for(set<int>::iterator ii=s.begin(); ii!=s.end(); ++ii)
    //  printf(" %d",*ii);
    //printf("\n");

    /* Create the set of all triggers for transitions from these states,
       keeping similarly-behaving phones collected as much as possible.
       Also handle the special conditions on other forms of states.

       It's critical to handle zeros specially; we don't treat them here.  */
    vector<forfc<string> > set0, set1, *tr_old = &set0, *tr_new = &set1, *tr_tmp;
    tr_old->push_back(forfc<string>("0", false));
    
    for(set<int>::iterator ii=s.begin(); ii!=s.end(); ++ii) {
      int i=*ii%n, form=*ii/n;
      forfc<string> trigger_union(vector<string>(0), false);
      for(int j=q[i].t.size()-1; j>=0; j--) {
        forfc<string> x = q[i].t[j]->trigger_set(), y, z;
        tr_new->clear();
        for(vector<forfc<string> >::iterator jj=tr_old->begin(); jj!=tr_old->end(); ++jj) {
          y = forfc<string>(*jj); y.intersect(x);
          z = forfc<string>(*jj); z.subtract(x);
          if (!y.empty() && !(form==2 && q[i].t[j]->d == q1 && not_sporadic)) {
            tr_new->push_back(y);
            trigger_union.subtract(x);
          }
          if (!z.empty())
            tr_new->push_back(z);
        }
        tr_tmp = tr_old; tr_old = tr_new; tr_new = tr_tmp;
      }
      /* Handle the special case for form 1 states.  Note that since we've already split
         on all the things whose union is the complement of trigger_union, everything
         is either contained in it or disjoint from it.  The ones contained in it,
         i.e. the ones such that subtracting it leaves them empty, should perish.  */
      if (form == 1) 
        for(int k = tr_old->size()-1; k>=0; k--) {
          (*tr_old)[k].subtract(trigger_union);
          if ((*tr_old)[k].empty()) {
            (*tr_old)[k] = (*tr_old)[tr_old->size()-1];
            tr_old->pop_back();
          }
        }      
    }

    for(vector<forfc<string> >::iterator jj=tr_old->begin(); jj!=tr_old->end(); ++jj) {
      /* A representative phone from this set.  If it's infinite, we use "*",
         which is certainly not a phone (because our syntax prevents it), and so
         in particular is not in any other set.  */
      string r = jj->pos ? jj->s[0] : "*";
      set<pair<int,vector<string> > > t; // the set of reachable states, with forms and writings
      //printf("entering transition generation for the class represented by %s\n", r.c_str());
      //printf("this class %s", jj->pos ? "contains" : "doesn't contain");
      //for (int k = jj->s.size()-1; k>=0; k--)
      //  printf(" %s", jj->s[k].c_str());
      //printf("\n");
      
      /* Multiple outcomes are necessary if:
         - this state is form 0, and some outgoing transitions rewrite;
         - this state is form 1, and there are multiple modifications with the
           same trigger (in form 2, we instead transition to all of them).
         Our handling of multiple outcomes is uglyish.  */
      /* Also, don't add any forms of the destination state q1.  */
      // stores outputs and extra sets for multiple outcomes -- and how 'bout that type?
      vector<pair<vector<string>, set<pair<int,vector<string> > > > > mult; 
      int mult_state = -1; // if not -1, then the extended state which needed multiplicity
      vector<string> mult_string; // the outcome of this transition
      bool mult_string_valid = false;
      // stores alternatives for zero transitions from form 1 states
      vector<set<pair<int,vector<string> > > > zero_mult;
      bool last_special = false;

      for(set<int>::iterator ii=s.begin(); ii!=s.end(); ++ii) {
        int i=*ii%n, form=*ii/n;
        set<pair<int,vector<string> > > magic;
        bool use_magic = true, took = false; 
        //printf("in the mess for %d (state %d, form %d)\n", *ii, i, form);
        for(int j=q[i].t.size()-1; j>=0; j--) {
          //printf("the transition ", j);
          //q[i].t[j]->display();
          //printf("\n");
          if (q[i].t[j]->trigger_set().contains(r)) {
            //printf("trigger set contains it\n");
            if((form == 1 || form == 0) && (q[i].t[j]->kind() == POS_TR || q[i].t[j]->kind() == NER_TR)) {
              //printf("form is multiplicative\n");

              /* Fail if there's another nontrivial source of multiplicity, because
                 this guarantees ambiguity.  */
              if (mult_state != -1 && mult_state != *ii) {
                if (respecting_conflicts)
                  return NULL;
                mult.clear();
              }
              mult_state = *ii;

              set<pair<int,vector<string> > > w;
              /* Default w to {(-1,[])}, which is a fake state we can recognize.
                 Empty sets don't work for the Cartesian product later.  */
              if(q[i].t[j]->d != q1) {
                w = zero_closure[q[i].t[j]->d + n]; // n*1
                magic.insert(zero_closure[q[i].t[j]->d + n*2].begin(),
                             zero_closure[q[i].t[j]->d + n*2].end());
              }
              else {
                w.insert(pair<int,vector<string> >(-1, vector<string>()));
                use_magic = false;
              }
              if (form == 0)
                took = true;

              vector<vector<string> > outcomes;
              if (jj->pos) {
                vector<string> one_outcome = vector<string>(jj->s.size());
                vector<int> iv = vector<int>(jj->s.size()), iu = vector<int>(jj->s.size());
                int l;
                for(l=iv.size()-1; l>=0; l--) {
                  iv[l] = 0; iu[l] = q[i].t[j]->all_outcomes(jj->s[l]).size();
                }
                do {
                  for(int k=jj->s.size()-1; k>=0; k--)
                    one_outcome[k] = q[i].t[j]->all_outcomes(jj->s[k])[iv[k]];
                  outcomes.push_back(one_outcome);
                  for(l=iv.size()-1; l>=0 && ++iv[l]>=iu[l]; l--) iv[l] = 0;
                } while (l >= 0);
              }
              else
                outcomes = vector<vector<string> >(1, q[i].t[j]->all_outcomes(r));

              for(vector<vector<string> >::iterator oi=outcomes.begin(); oi!=outcomes.end(); ++oi)
                mult.push_back(pair<vector<string>, set<pair<int,vector<string> > > >(*oi, w));
            }
            else { 
              //printf("form is innocuous, or transition is nonrewriting\n");
              if(q[i].t[j]->d != q1) {
                if (form == 1)
                  zero_mult.push_back(zero_closure[q[i].t[j]->d + n*form]);
                else
                  t.insert(zero_closure[q[i].t[j]->d + n*form].begin(),
                           zero_closure[q[i].t[j]->d + n*form].end());
              }
            }
          }
        } // for j
          /* this is the constant transformation: if jj->s is finite, it's represented by
             jj->s, else by the vector containing our fake phone "*".  */
        if (use_magic && took) {
            //printf("form was special, so we insert the nonchanging case\n");
            mult.push_back(pair<vector<string>, set<pair<int,vector<string> > > >
                           (jj->pos ? jj->s : vector<string>(1, "*"), magic));
            last_special = true;
        }

        /* Collapse mult if its size is 1, and reset mult_state.  Test for mult_string failures.
           If on the other hand mult has size exceeding 1, we assume that the resulting outcomes
           are always different, so that it's never collapsible.

           In multiplicity handling here and above, if we're simply ignoring conflicts,
           then we simply discard the current mult when something else arises.  */
        if (mult_string_valid && mult.size() > 1) {
          if (respecting_conflicts)
            return NULL;
          mult.clear();
          mult_state = -1;
        }
        else if (mult_string_valid && mult.size() == 1 && mult.begin()->first != mult_string) {
          if (respecting_conflicts)
            return NULL;
          mult.clear();
          mult_state = -1;
        }
        if (mult.size() == 1) {
          //printf("converting from a multiplicity of size 1\n");
          if (last_special)
            t.insert(mult.begin()->second.begin(), mult.begin()->second.end());
          else
            zero_mult.push_back(mult.begin()->second);
          last_special = false;
          mult_state = -1;
          mult_string = mult.begin()->first;
          mult_string_valid = true;
          mult.clear();
        }
      } // for j

      /* Create the transitions in b.  */
      vector<pair<vector<string>, set<pair<int,vector<string> > > > >::iterator kk = mult.begin(),
        lastkk = mult.end();
      lastkk--;
      do {
        set<pair<int,vector<string> > > t1(t);
        vector<set<pair<int,vector<string> > > > zero_mult0(zero_mult);
        vector<string> outcomes;
        if (mult_state != -1) {
          if (last_special && kk == lastkk)
            t1.insert(kk->second.begin(), kk->second.end());
          else
            zero_mult0.push_back(kk->second);
          outcomes = kk->first;
        }
        else if (mult_string_valid)
          outcomes = mult_string;
        else
          outcomes = vector<string>(1, "*"); // for constant

        /* Loop over all zero outcomes for type 1 states.  */
        vector<set<pair<int,vector<string> > >::iterator> iiv(zero_mult0.size());
        int iiv_i;
        for(int i=0; i<iiv.size(); i++)
            iiv[i] = zero_mult0[i].begin();
        do { 
          set<pair<int,vector<string> > > t0(t1);
          /* Don't insert -1s; they're not for real.  */
          for(int i=iiv.size()-1; i>=0; i--)
            if (iiv[i]->first != -1)
              t0.insert(*iiv[i]);
          
          transition *tr;
          if(jj->pos) {
            if(outcomes[0] == "*")
              tr = new cst_transition(jj->s);
            else
              tr = new pos_transition(jj->s, outcomes);
          }
          else {
            if(outcomes[0] == "*")
              tr = new neg_transition(jj->s);
            else
              tr = new ner_transition(jj->s, outcomes[0]);
          }
          
          tr->d = b->zero_reach(t0, zero_closure_breaking, not_sporadic, label, m, queue, q1, n);
          b->q[label[s]].t.push_back(tr);

          /* Prepare for the next set of zero transitions for form 1 states.  */
          for(iiv_i = iiv.size()-1; iiv_i>=0 && ++iiv[iiv_i]==zero_mult0[iiv_i].end(); iiv_i--)
            iiv[iiv_i] = zero_mult0[iiv_i].begin();
        } while (iiv_i >= 0);
      } while (mult_state != -1 && ++kk != mult.end());
      
    } // for jj
  } // while queue nonempty

  /* Last step: do some trimming.  First remove zero-transitions to the same state.
     Then remove states that are nonaccepting and have no transitions to a different state
     (this isn't quite all that could be done, but it catches a lot).  */
  for(int i=b->q.size()-1; i>=0; i--)
    for(int j=b->q[i].t.size()-1; j>=0; j--) {
      if(b->q[i].t[j]->kind() == CST_TR && ((cst_transition *)b->q[i].t[j])->x == vector<string>(1, "0") &&
         b->q[i].t[j]->d == i) {
        delete b->q[i].t[j];
        b->q[i].t[j] = b->q[i].t[b->q[i].t.size()-1];
        b->q[i].t.pop_back();
      }
      else if (b->q[i].t[j]->kind() == POS_TR && ((pos_transition *)b->q[i].t[j])->x == vector<string>(1, "0") &&
               ((pos_transition *)b->q[i].t[j])->y == vector<string>(1, "0") && b->q[i].t[j]->d == i) {
        delete b->q[i].t[j];
        b->q[i].t[j] = b->q[i].t[b->q[i].t.size()-1];
        b->q[i].t.pop_back();
      }       
    }

  /* temporary!!!!!!!!!!!!!!!! */
  return b;

  bool any_pruned;
  do {
    any_pruned = false;
    for(int i=b->q.size()-1; i>=0; i--)
      if (!b->q[i].accept) {
        bool no_transition_out = true;
        for(int j=b->q[i].t.size()-1; j>=0; j--)
          if(b->q[i].t[j]->d != i) {
            no_transition_out=false;
            break;
          }
        if(no_transition_out) {
          b->delete_state(i);
          any_pruned = true;
        }
      }
  } while (any_pruned);

  return b;
}



/* Do the zero application thing.  */
void automaton::apply_zeros(const application *a, set<application> *s, vector<int> &c, int max_epen) {
  vector<int> d(c);
  
  /* Look for transitions triggered by _finite sets_ including 0.
     If there are none, or if we've exceeded max_epen visits here,
     insert this application and stop.  Otherwise recurse onto all
     these transitions.  */
  d[a->q]++;
  if(d[a->q] > max_epen) {
    s->insert(*a);
    return;
  }

  string zero = "0";
  for(int j=q[a->q].t.size()-1; j>=0; j--)
    if(q[a->q].t[j]->trigger_set().contains(zero) &&
       (q[a->q].t[j]->kind() == POS_TR || q[a->q].t[j]->kind() == CST_TR)) {
      vector<string> u = q[a->q].t[j]->all_outcomes("0"); 
      for(int k=u.size()-1; k>=0; k--) {
        vector<string> w(a->y);
        if (u[k] != "0") {
          w.push_back(u[k]);
        }
        application b(w, q[a->q].t[j]->d);
        apply_zeros(&b, s, d, max_epen);
      }
    }

  s->insert(*a);
}

/* Return the set of all strings that a given string transduces to.  */
set<vector<string> > *automaton::transduce(vector<string> *x, int max_epen, bool reflect) {
  if (reflect)
    reverse(x->begin(), x->end());

  set<application> s0, s1, s2, *s_old = &s0, *s_new = &s1, *s_mid = &s2;
  s_old->insert(application(vector<string>(0), q0));

  //printf("initially: ");
  //s_old->begin()->display();
  //printf("\n");
  
  for(int i=0; ; i++) {
    //printf("on phone %d\n", i);
    /* First check for zero transitions triggered by _finite sets_,
       and do all combinations of these
       that present themselves, subject to the condition that, after having
       returned to a state for the > max_epen th time, we stop.
       Put the results in s_mid.  */
    for(set<application>::iterator ii=s_old->begin(); ii!=s_old->end(); ++ii) {
      vector<int> c(q.size(), 0);
      apply_zeros(&*ii, s_mid, c, max_epen);
    }

    //printf("after zeros:");
    //for(set<application>::iterator ii=s_mid->begin(); ii!=s_mid->end(); ++ii) {
    //  printf(" ");
    //  ii->display();
    //}
    //printf("\n");

    if (i == x->size())
      break;

    string r = (*x)[i];
    //printf("the phone is \"%s\"\n", r.c_str());
    /* Apply all transitions with the trigger r.  */
    for(set<application>::iterator ii=s_mid->begin(); ii!=s_mid->end(); ++ii)
      for(int j=q[ii->q].t.size()-1; j>=0; j--)
        if(q[ii->q].t[j]->trigger_set().contains(r)) {
          vector<string> u = q[ii->q].t[j]->all_outcomes(r); 
          for(int k=u.size()-1; k>=0; k--) {
            vector<string> w(ii->y);
            if (u[k] != "0")
              w.push_back(u[k]);
            s_new->insert(application(w, q[ii->q].t[j]->d));
          }
        }

    //printf("after nonzeros:");
    //for(set<application>::iterator ii=s_new->begin(); ii!=s_new->end(); ++ii) {
    //  printf(" ");
    //  ii->display();
    //}
    //printf("\n");

    set<application> *s_tmp;
    s_old->clear(); s_mid->clear();
    s_tmp = s_old; s_old = s_new; s_new = s_tmp;
  }

  set<vector<string> > *s = new set<vector<string> >;
  for(set<application>::iterator ii=s_mid->begin(); ii!=s_mid->end(); ++ii)
    if (q[ii->q].accept) {
      vector<string> v = ii->y;
      if (reflect)
        reverse(v.begin(), v.end());
      s->insert(v);      
    }
      
  return s; 
}




