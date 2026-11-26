// determine number of turns before first charged attack
#include "html.h"
#include <vector>
#include <cassert>
#include <algorithm>

// get time to first and damage for all fast+charged pairs
static void
calctimetoallNx1(std::vector<timetofirst> &ttfs, std::vector<species> &megaspecs){
  for(auto s = species_begin() ; s != species_end() ; ++s){
    calctimetosNx1(ttfs, *s, false);
    for(const auto &m : s->mforms){
      megaspecs.emplace_back(&*s, m);
    }
  }
  for(const auto &s : megaspecs){
    calctimetosNx1(ttfs, s, false);
  }
}

[[noreturn]] static void usage(const char *argv0){
  std::cerr << "usage: " << argv0 << " [ extrema ]" << std::endl;
  exit(EXIT_FAILURE);
}

static void html_header(void){
  std::cout << "<table class=\"evenshade apair\">" << std::endl;
  std::cout << "<tr>";
  std::cout << "<th></th><th>Pokémon</th><th>Attack pair</th><th>Turns</th><th>Power</th><th><i>e</i></th><th>PPT</th><th>%c</th>";
  std::cout << "</tr>" << std::endl;
}

static void emit_row(const timetofirst &t){
  std::cout << "<tr>";
  std::cout << "<td>";
  html_types(t.s->t1, t.s->t2, false);
  std::cout << "</td>";
  std::cout << "<td>";
  link_to_name(std::cout, t.s->name, true);
  std::cout << "</td>";
  std::cout << "<td>";
  attack_with_icon(std::cout, *t.s, t.fa, true, false);
  std::cout << " + ";
  html_type(std::cout, t.ca->type, false);
  std::cout << "&nbsp;";
  emit_html_attack(std::cout, *t.s, t.ca, true);
  std::cout << "</td>";
  std::cout << "<td>" << t.turns << "</td>";
  std::cout.precision(1);
  std::cout << "<td>" << t.dam << "</td>";
  std::cout.precision(2);
  std::cout << "<td>";
  if(t.excesse){
    std::cout << t.excesse;
  }
  std::cout << "</td>";
  std::cout << "<td>" << t.dpt << "</td>";
  std::cout << "<td>" << (t.powercharged * 100 / t.dam) << "</td>";
  std::cout << "</tr>" << std::endl;
}

// table of all cycles using Nx1 stats, in HTML
int main(int argc, char **argv){
  if(argc != 1){
    usage(argv[0]);
  }
  std::vector<timetofirst> ttfs;
  html_header();
  std::vector<species> megaspecs; // backing storage for species boosted from megas
  calctimetoallNx1(ttfs, megaspecs);
  std::sort(ttfs.begin(), ttfs.end());
  std::cout.setf(std::ios::fixed, std::ios::floatfield);
  std::cout.precision(2);
  std::cout << std::noshowpoint;
  unsigned fastest = 0;
  for(const auto &t : ttfs){
    if(!fastest){
      fastest = t.turns;
    }
    emit_row(t);
  }
  std::cout << "</table>" << std::endl;
  return EXIT_SUCCESS;
}
