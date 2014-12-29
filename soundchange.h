#ifndef __RSCA_SOUNDCHANGE
#define __RSCA_SOUNDCHANGE

#include <string>

using namespace std;

struct change_parameters {
  string name;
  int max_epen;
  bool not_sporadic;
  bool respecting_conflicts;
  bool reflect;
  
  change_parameters() {
    name = "";
    max_epen = 1;
    not_sporadic = true;
    respecting_conflicts = true;
    reflect = false;
  }
};

#endif
