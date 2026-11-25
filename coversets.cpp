#include "html.h"
#include <getopt.h>
#include <memory>
#include <algorithm>

using tarray = std::array<bool, TYPECOUNT>;
using typingvec = std::vector<std::pair<pgo_types_e, pgo_types_e>>;

// coverset detection algorithm:
//
// generate all typings in the population
// start with no arrays
// for each member M of the typing population
//  for each array A
//   can A already hit M at the required level? if so, loop
//   for each type T that can hit M at required level
//     extend copy of A with T, check that it's not already in list
//      if it is, delete it
//      if it is not, add it to list
//   delete A from list
// sort by size, delete all that are not minimal


// tocc -- array indicating membership of attack types in this coverset
static void
emit_coverset(const std::vector<pgo_types_e> &tocc, int setidx, bool html){
  if(html){
    std::cout << "<tr><td>" << setidx << "</td><td>";
    for(auto p : tocc){
      html_type(std::cout, p, false);
      std::cout << ' ';
    }
    std::cout << "</td></tr>" << std::endl;
  }else{
    printf(" cover: %d ", setidx);
    for(auto p : tocc){
      printf("%s ", tnames[p]);
    }
    printf("\n");
  }
}

// sort the coversets so that the most common types are to the left and ideally
// in a single column. in particular, any type that shows up in all the coversets
// must be in the same column in all of them.
// out is a set of size equal to tarrays, but vectors of types having size equal
// to the coverset size (i.e. number of occupied elements in the tarrays), sorted.
// at the conclusion, out is populated, and tarrays is reset to all 0s.
static void
organize_coversets(std::vector<tarray> &tarrays, std::vector<std::vector<pgo_types_e>> &out){
  int counts[TYPECOUNT]{};
  int maxcount = 0; // count of whichever type shows up the most, <= tarrays.size
  pgo_types_e bigtype = TYPECOUNT; // type corresponding to maxcount
  unsigned setsize = 0;
  for(const auto &t : tarrays){
    for(pgo_types_e p = TYPESTART ; p < TYPECOUNT ; ++p){
      if(t[p]){
        ++setsize;
        if(++counts[p] > maxcount){
          maxcount = counts[p];
          bigtype = p;
        }
      }
    }
  }
  if(!setsize){
    std::cerr << "coversets were empty" << std::endl;
    throw std::exception();
  }
  if(setsize % tarrays.size()){
    std::cerr << "coversets varied in size" << std::endl;
    throw std::exception();
  }
  setsize /= tarrays.size();
  // bubble each column. first will be bigtype.
  while(bigtype != TYPECOUNT){
//std::cerr << tname_capitalized(bigtype) << " count: " << counts[bigtype] << std::endl;
    for(unsigned i = 0 ; i < tarrays.size() ; ++i){
      if(tarrays[i][bigtype]){
        out[i].push_back(bigtype);
      }
    }
    counts[bigtype] = 0;
    maxcount = 0;
    bigtype = TYPECOUNT;
    for(pgo_types_e p = TYPESTART ; p < TYPECOUNT ; ++p){
      if(counts[p] > maxcount){
        maxcount = counts[p];
        bigtype = p;
      }
    }
  }
}

static void
emit_coversets(std::vector<tarray> &tarrays, const std::string &title, unsigned pop, bool html){
  if(tarrays.empty()){
    return;
  }
  std::vector<std::vector<pgo_types_e>> out(tarrays.size());
  organize_coversets(tarrays, out);
  if(html){
    std::cout << "<h2>" << title << " (" << pop << " participants)</h2>" << std::endl;
    std::cout << "<table class=\"coverset\">" << std::endl;
    std::cout << "<tr><th></th><th></th></tr>" << std::endl;
  }else{
    std::cout << "population: " << pop << std::endl;
  }
  int setidx = 1;
  for(const auto &clean : out){
    emit_coverset(clean, setidx, html);
    ++setidx;
  }
  if(html){
    std::cout << "</table>" << std::endl;
  }
}

// level is the filter of effectiveness -- we must be greater
// tocc: TYPECOUNT-sized vector of ints -- our attacking types
//  print tocc iff it covers tvec
// tvec is a v-sized vector of 2-vectors of ints (2v total elements) -- our target typings
//  if the first int is TYPECOUNT, it's not populated (we don't care)
//  we must cover each valid typing at level + 1
static int
print_coverset(const tarray &tocc, const typingvec &tvec, int level,
               std::vector<tarray> &setvec){
  for(unsigned i = 0 ; i < tvec.size() ; ++i){
    if(tvec[i].first != TYPECOUNT){ // it's populated; ensure we cover
      int covered = 0;
      for(pgo_types_e j = TYPESTART ; j < TYPECOUNT ; ++j){
        if(tocc[j]){ // we have this attacking type; check the typing relation
          int tr = typing_relation(j, tvec[i].first, tvec[i].second);
          if(tr > level){
            covered = 1;
            break;
          }
        }
      }
      if(!covered){
        return 0;
      }
    }
  }
  // we found a coverset, yay
  setvec.emplace_back(tocc);
  return 1;
}

// we are placing n 1s among the last m slots of tocc
static int
place_n_in_m(int n, int m, const tarray &tocc, const typingvec &tvec,
             int level, std::vector<tarray> &setvec){
  tarray tcopy(tocc);
  // base case m == 1
  if(m == 1){
    if(n){
      tcopy[TYPECOUNT - m] = 1;
    }
    int r = print_coverset(tcopy, tvec, level, setvec);
    return r;
  }
  int ret = 0;
  // test with our bit off unless we must place this bit
  if(n < m){
    ret += place_n_in_m(n, m - 1, tcopy, tvec, level, setvec);
  }
  // test with our bit on if we have a bit to place
  if(n){
    tcopy[TYPECOUNT - m] = 1;
    ret += place_n_in_m(n - 1, m - 1, tcopy, tvec, level, setvec);
  }
  return ret;
}

// print the sets of size n or less which cover the specified vector
static int
gen_coversets(int n, const typingvec &tvec, int level,
              std::vector<tarray> &setvec){
  tarray tocc{};
  return place_n_in_m(n, TYPECOUNT, tocc, tvec, level, setvec);
}

static void
print_participant_attacks(const species &s, bool fast){
  bool one = false;
  bool atypes[TYPECOUNT] = {};
  for(const auto a : s.attacks){
    if(fast_attack_p(*a) != fast){
      continue;
    }
    if(a->type != TYPECOUNT){
      if(atypes[a->type]){
        continue;
      }
    }
    if(one){
      std::cout << ", ";
    }
    if(a->type == TYPECOUNT){
      std::cout << "HiddenPower";
    }else{
      std::cout << tname_capitalized(a->type);
      atypes[a->type] = true;
    }
    one = true;
  }
}

static void
print_participant(const species &s){
  std::cout << " " << s.name << " (" << tname_capitalized(s.t1);
  if(s.t2 != TYPECOUNT){
    std::cout << ", " << tname_capitalized(s.t2);
  }
  std::cout << ") → ";
  print_participant_attacks(s, true);
  std::cout << " : ";
  print_participant_attacks(s, false);
  std::cout << std::endl;
}

// returns true if s ought be in the population according to the
// rules of generate_population()
static bool
check_pop_for_form(const tarray &want, const tarray &exclude,
                   const std::vector<const species *> &informs,
                   const std::vector<const species *> &outforms,
                   const species &s){
  if(pvp_banned_p(s)){
    return false;
  }
  // first check outforms
  for(const auto out : outforms){
    if(s.name == out->name){
      return false;
    }
  }
  // check informs
  for(const auto in : informs){
    if(s.name == in->name){
      return true;
    }
  }
  // check exclude against both of s's types
  if(exclude[s.t1] || (s.t2 != TYPECOUNT && exclude[s.t2])){
    return false;
  }
  // check want against both of s's types
  if(want[s.t1] || (s.t2 != TYPECOUNT && want[s.t2])){
    return true;
  }
  return false;
}

// want: types to include (in)
// exclude: types to exclude, has priority over want (in)
// inspec: forms to include, has priority over want+exclude (in)
// outspec: forms to exclude, has priority over everything (in)
// bool: ought megas be considered
// pop: resulting collection of forms (out)
// store: backing store for pop (out)
static void
generate_population(const tarray &want, const tarray &exclude,
                    const std::vector<const species *> &informs,
                    const std::vector<const species *> &outforms,
                    bool megas, std::vector<const species *> &pop,
                    std::forward_list<species> &store){
  for(auto s = species_begin() ; s != species_end() ; ++s){
    if(check_pop_for_form(want, exclude, informs, outforms, *s)){
      pop.emplace_back(&*s);
    }
    if(megas){
      for(const auto &m : s->mforms){
        auto &smeg = store.emplace_front(&*s, m);
        if(check_pop_for_form(want, exclude, informs, outforms, smeg)){
          pop.emplace_back(&smeg);
        }
      }
    }
  }
}

// extract all the typings from the generated population.
// typepop will be a set of them with no duplicates.
static void
generate_targtypes(const std::vector<const species *> &pop,
                   std::vector<std::pair<pgo_types_e, pgo_types_e>> &typepop){
  for(const auto *s : pop){
    bool found = false;
    for(const auto &p : typepop){
      if((s->t1 == p.first && s->t2 == p.second) || (s->t1 == p.second && s->t2 == p.first)){
        found = true;
        break;
      }
    }
    if(!found){
      typepop.emplace_back(s->t1, s->t2);
    }
  }
}

// print coverset of all specified typings. t is a
// TYPECOUNT sized vector with 1 for the typings we care about, 0 otherwise.
static int
print_coversets_duals(const std::string &title, const std::vector<const species *> &pop,
                      const std::vector<std::pair<pgo_types_e, pgo_types_e>> &tpop,
                      int level, bool html){
  std::vector<tarray> setvec;
  for(int j = 1 ; j <= TYPECOUNT ; ++j){
    int min = gen_coversets(j, tpop, level, setvec);
    if(min){
      fprintf(stderr, "%d dual coversets of size %d\n", min, j);
      if(!html){
        for(const auto *s : pop){
          print_participant(*s);
        }
      }
      emit_coversets(setvec, title, pop.size(), html);
      return 0;
    }else{
      fprintf(stderr, "no dual coversets of size %d\n", j);
    }
  }
  std::cout << "couldn't find coverset" << std::endl;
  return -1;
}

[[noreturn]] static void
usage(const char *argv0){
  std::cerr << "usage: " << argv0 << " [ options... ] " << std::endl;
  std::cerr << " poptype[,poptype...]: require one of these types" << std::endl;
  std::cerr << " -x poptype[,poptype...]: exclude these types" << std::endl;
  std::cerr << " -X form: exclude this form, may be supplied multiple times" << std::endl;
  std::cerr << " -n name: name of coverset (for html)" << std::endl;
  std::cerr << " -l type effectiveness filter (default 0)" << std::endl;
  std::cerr << " -h: generate html" << std::endl;
  std::cerr << " -m: allow mega evolutions" << std::endl;
  std::cerr << " -t charged: require a charged attack of this type" << std::endl;
  exit(EXIT_FAILURE);
}

static int
lex_type(const char* tstr, tarray &tlist){
  pgo_types_e t = lookup_type(tstr);
  if(t == TYPECOUNT){
    std::cerr << "couldn't look up type for " << tstr << std::endl;
    return -1;
  }
  if(tlist[t]){
    std::cerr << "already provided type " << tstr << std::endl;
    return -1;
  }
  tlist[t] = 1;
  return 0;
}

// lex a comma-delimited listed of types. it is an error to be empty, or to
// repeat a type, or to specify an invalid type. case is ignored.
static int
lex_typelist(const char* arg, tarray &tlist){
  for(unsigned z = 0 ; z < TYPECOUNT ; ++z){
    tlist[z] = 0;
  }
  const char* start = arg;
  unsigned tcount = 0;
  char curtype[20]; // enough to hold all type names (and then some)
  unsigned curl = 0; // length in curtype
  while(*arg){
    if(isalpha(*arg)){
      if(curl >= sizeof(curtype) / sizeof(*curtype)){
        std::cerr << "invalid type: " << start << std::endl;
        return -1;
      }
      curtype[curl++] = tolower(*arg);
    }else if(*arg == ','){
      curtype[curl] = '\0';
      if(lex_type(curtype, tlist)){
        return -1;
      }
      ++tcount;
      curl = 0;
    }else{
      std::cerr << "invalid type: " << start << std::endl;
      return -1;
    }
    ++arg;
  }
  if(curl){
    curtype[curl] = '\0';
    if(lex_type(curtype, tlist)){
      return -1;
    }
  }else if(tcount == 0){
    std::cerr << "empty typelist: " << start << std::endl;
    return -1;
  }
  return 0;
}

int main(int argc, char* const* argv){
  tarray kern, outkern{}, reqchargedtype;
  const char *title = nullptr;
  const char *argv0 = argv[0];
  bool exclude = false;
  bool reqcharged = false; // FIXME not yet implemented
  bool usemegas = false;
  bool html = false;
  int level = 0;
  int go;
  std::vector<const species *> excludelist;
  while((go = getopt(argc, argv, ":hl:n:X:x:t:m")) > 0){
    switch(go){
      case 'X':{ // exclude this form
        const auto s = lookup_species(optarg);
        if(!s){
          std::cerr << "couldn't find form: " << optarg << std::endl;
          usage(argv0); break;
        }
        if(std::ranges::contains(excludelist, s)){
          std::cerr << "specified form twice: " << optarg << std::endl;
          usage(argv0); break;
        }
        excludelist.push_back(s);
        break;
      }case 'x': // exclude anything containing these types
        if(exclude){
          std::cerr << "can't provide -x twice" << std::endl;
          usage(argv0); break;
        }
        if(lex_typelist(optarg, outkern)){
          usage(argv0); break;
        }
        exclude = true;
        break;
      case 'l': // level filter
        if(level){
          std::cerr << "can't provide -l twice" << std::endl;
          usage(argv0); break;
        }
        level = atoi(optarg);
        if(level == 0){
          std::cerr << "invalid level: " << optarg << std::endl;
          usage(argv0); break;
        }
        break;
      case 't': // show population members that can use these types of charged attacks
        fprintf(stderr, "-t is not yet supported\n"); // FIXME
        if(reqcharged){
          std::cerr << "can't provide -t twice" << std::endl;
          usage(argv0); break;
        }
        if(lex_typelist(optarg, reqchargedtype)){
          usage(argv0); break;
        }
        reqcharged = true;
        break;
      case 'n':
        if(title){
          std::cerr << "can't provide -n twice" << std::endl;
          usage(argv0); break;
        }
        title = optarg;
        break;
      case 'm':
        if(usemegas){
          std::cerr << "can't provide -m twice" << std::endl;
          usage(argv0); break;
        }
        usemegas = true;
        break;
      case 'h':
        html = true;
        break;
      case ':':
        std::cerr << "option requires argument: " << std::endl;
        usage(argv0); break;
      case '?':
        std::cerr << "unknown argument: " << std::endl;
        usage(argv0); break;
    }
  }
  // if an argument remains, it ought be a typelist specifying required typings
  if(*(argv + optind)){
    if(*(argv + optind + 1)){
      std::cerr << "too many arguments" << std::endl;
      usage(argv0);
    }
    if(lex_typelist(*(argv + optind), kern)){
      usage(argv0);
    }
  }else{
    for(pgo_types_e t = TYPESTART ; t < TYPECOUNT ; ++t){
      kern[t] = true;
    }
  }
  if(!title){
    title = "Custom";
  }
  std::forward_list<species> store;
  std::vector<const species *> pop;
  std::vector<std::pair<pgo_types_e, pgo_types_e>> typepop;
  generate_population(kern, outkern, {}, excludelist, usemegas, pop, store);
  generate_targtypes(pop, typepop);
  print_coversets_duals(title, pop, typepop, level, html);
  return EXIT_SUCCESS;
}
