#include "html.h"
#include <set>
#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include <unordered_map>

static int
write_iv_graph(const species &s, const char *leagueshort){
  bool graph = !s.name.contains("G-Max"); // FIXME
  if(graph){
    std::string cmd = "Rscript ../../gmean.R \"" + s.name + "\" " + leagueshort;
    if(system(cmd.c_str())){ // ugh
      std::cerr << "Rscript gmean failed for " << s.name << leagueshort << std::endl;
      return -1;
    }
    /*std::string radar = "Rscript ../../optstatsradar.R \"" + s.name + "\" " + leagueshort;
    if(system(radar.c_str())){ // ugh
      std::cerr << "Rscript radar failed for " << s.name << std::endl;
      return -1;
    }*/
  }
  return 0;
}

static int
write_pacing(const attack *fa, const attack *c1, const attack *c2){
  std::string pacing = "cd graphs && ../../../out/pacing \"" +
    fa->name + "\" \"" + c1->name + "\" \"" + c2->name + "\"";
  std::cout << pacing << std::endl;
  if(system(pacing.c_str())){
    std::cerr << "pacing failed for \"" << pacing << "\"" << std::endl;
    return -1;
  }
  return 0;
}

// keep track of which pacing diagrams we've already drawn. hash on the
// fast attack, then hash on the first (lexicographically) charged attack,
// then check the set for membership.
using atkset = std::set<const attack *>;
using catkhash = std::unordered_map<const attack *, atkset>;

std::unordered_map<const attack *, catkhash> pacinghash;

static void
write_iv_graphs(const species &s){
  for(const auto *a : s.attacks){
    if(!fast_attack_p(*a)){
      continue;
    }
    auto chash = pacinghash.try_emplace(a);
    for(const auto *ca : s.attacks){
      if(!charged_attack_p(*ca)){
        continue;
      }
      auto cset = chash.first->second.try_emplace(ca);
      for(const auto *ca2 : s.attacks){
        if(ca2->name <= ca->name){
          continue;
        }
        if(!charged_attack_p(*ca2)){
          continue;
        }
        auto c2res = cset.first->second.emplace(ca2);
        if(c2res.second){
          if(write_pacing(a, ca, ca2)){
            throw std::exception();
          }
        }
      }
    }
  }
  if(usable_in_gl(s)){
    if(write_iv_graph(s, "GL")){
      throw std::exception();
    }
    std::cout << "Drew " << s.name << " GL" << std::endl;
  }
  if(usable_in_ul(s)){
    if(write_iv_graph(s, "UL")){
      throw std::exception();
    }
    std::cout << "Drew " << s.name << " UL" << std::endl;
  }
  if(usable_in_ml(s)){
    if(write_iv_graph(s, "ML")){
      throw std::exception();
    }
    std::cout << "Drew " << s.name << " ML" << std::endl;
  }
}

static int
write_mon_graphs(const species &s){
  write_iv_graphs(s);
  return 0;
}

static void
usage(const char *argv0, int ret){
  std::cerr << "usage: " << argv0 << " targetdir" << std::endl;
  exit(ret);
}

// generate a directory full of species graphs, overwriting any that exist.
int main(int argc, const char **argv){
  if(argc != 2){
    usage(argv[0], EXIT_FAILURE);
  }
  if(chdir(argv[1])){
    std::cerr << "error changing directory to " << argv[1] << ": " << strerror(errno) << std::endl;
    usage(argv[0], EXIT_FAILURE);
  }
  // FIXME parallelize this abortion
  std::vector<species> megas;
  for(auto s = species_begin() ; s != species_end() ; ++s){
    if(pvp_banned_p(*s)){
      continue;
    }
    if(write_mon_graphs(*s)){
      return EXIT_FAILURE;
    }
    for(const auto &m : s->mforms){
      megas.emplace_back(&*s, m);
    }
  }
  for(const auto &speg : megas){
    if(write_mon_graphs(speg)){
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
