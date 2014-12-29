#ifndef __RSCA_FORFC
#define __RSCA_FORFC

#include <vector>

using namespace std;

/* This class represents a set of Ts which is either finite or
   has a finite complement.  It uses vectors, not because that's
   especially efficient or appropriate, but because I had already
   implemented something like this with vectors.  */
template <class T>
struct forfc {
  vector<T> s;
  bool pos; // true if this represents s, false if the complement of s

  forfc() {}
  forfc(vector<T> s_, bool pos_ = true) : s(s_), pos(pos_) {}
  forfc(T a, bool pos_ = true) : pos(pos_) { s = vector<T>(1, a); }

  void complement() {
    pos = !pos;
  }

  void intersect(forfc<T> &t) {
    /* It might prove bad to mix up the order of phonesets like this.
    If so, I'll introduce iterators.  */
    if (pos)
      for(int i=s.size()-1; i>=0; i--) {
        bool not_in_t = (find(t.s.begin(), t.s.end(), s[i]) == t.s.end());
        if ((t.pos && not_in_t) || (!t.pos && !not_in_t)) {
          s[i] = s[s.size()-1];
          s.pop_back();
        }
      }
        else if (t.pos) {
          vector<string> u(s);
          s = t.s;
          for(int i=s.size()-1; i>=0; i--)
            if (find(u.begin(), u.end(), s[i]) != u.end()) {
              s[i] = s[s.size()-1];
              s.pop_back();
            }
              pos = true;
        }
        else {
          for(int i=t.s.size()-1; i>=0; i--)
            if (find(s.begin(), s.end(), t.s[i]) == s.end()) {
              s.push_back(t.s[i]);
            }
        }
  }

  void subtract(forfc<T> &t) {
    forfc<T> tc(t); tc.complement();
    intersect(tc);
  }

  bool empty() {
    return pos && s.empty();
  }

  bool contains(T x) {
    bool n = (find(s.begin(), s.end(), x) != s.end());
    return pos ? n : !n;
  }
};

#endif

