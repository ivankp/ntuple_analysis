#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <optional>
#include <algorithm>
#include <functional>
#include <regex>

#include <TFile.h>
#include <TKey.h>
#include <TChain.h>
#include <TH1.h>

#include <fastjet/ClusterSequence.hh>

#include <nlohmann/json.hpp>

#include "ivanp/string.hh"
#include "ivanp/branch_reader.hh"
#include "reweighter.hh"
#include "json/reweighter.hh"
#include "json/fastjet.hh"
#include "ivanp/tcnt.hh"
#include "ivanp/hist/histograms.hh"
#include "json/binning.hh"
#include "ivanp/vec4.hh"
#include "Higgs2diphoton.hh"
#include "ivanp/ycombinator.hh"

#define STR1(x) #x
#define STR(x) STR1(x)

#define TEST(var) std::cout << \
  "\033[33m" STR(__LINE__) ": " \
  "\033[36m" #var ":\033[0m " << (var) << std::endl;

using std::cout;
using std::cerr;
using std::endl;
using nlohmann::json;
using ivanp::cat; // concatenates strings
using ivanp::vec4;
using ivanp::branch_reader;

using namespace ivanp::cont::ops::map;

json read_json(const char* filename) {
  try {
    std::ifstream f;
    f.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    f.open(filename);
    json j;
    f >> j;
    return j;
  } catch (const std::exception& e) {
    cerr << "Cannot open json file " << filename << '\n';
    throw;
  }
}
const json& get(const json& j) {
  return j;
}
template <typename T, typename... Ts>
const json& get(const json& j, const T& key, const Ts&... keys) {
  return get(j.at(key),keys...);
}
template <typename... Ts>
const auto& get_str(const json& j, const Ts&... keys) {
  return get(j,keys...).template get_ref<const json::string_t&>();
}
template <typename T, typename... Ts>
T get_val(T val, const json& j, const Ts&... keys) {
  try {
    return get(j,keys...).template get<T>();
  } catch (...) {
    return val;
  }
}

// Global event variables
std::vector<double> weights; // multiple weights per event
int event_id = -1;

struct initial_state {
  static constexpr const char* name = "initial_state";
  static constexpr std::array<const char*,4> tags {
    "all", "gg", "gq", "qq"
  };
  inline static unsigned index;
  static void set(int id1, int id2) noexcept {
    const bool g1 = (id1 == 21), g2 = (id2 == 21);
    index = ( g1!=g2 ? 2 : ( g1 ? 1 : 3 ) );
  }
};
template <typename Bin>
struct initial_state_tag: initial_state {
  std::array<Bin,tags.size()> bins;
  void operator+=(double w) noexcept {
    bins[0] += w;
    bins[index] += w;
  }
  void finalize() noexcept {
    for (auto& bin : bins)
      bin.finalize();
  }
  const Bin& operator[](size_t i) const noexcept { return bins[i]; }
};

struct photon_cuts {
  static constexpr const char* name = "photon_cuts";
  static constexpr std::array<const char*,2> tags {
    "all", "photons_pass"
  };
  inline static bool pass;
  static void set(bool _pass) noexcept {
    pass = _pass;
  }
};
template <typename Bin>
struct photon_cuts_tag: photon_cuts {
  std::array<Bin,tags.size()> bins;
  void operator+=(double w) noexcept {
    bins[0] += w;
    if (pass) bins[1] += w;
  }
  void finalize() noexcept {
    for (auto& bin : bins)
      bin.finalize();
  }
  const Bin& operator[](size_t i) const noexcept { return bins[i]; }
};

struct multiweight {
  static constexpr const char* name = "weight";
  inline static std::vector<std::string> tags;
};
template <typename Bin>
struct multiweight_tag: multiweight { // handle multiple weights
  std::vector<Bin> bins;

  multiweight_tag(): bins(weights.size()) { }
  void operator++() noexcept {
    for (size_t i = weights.size(); i--; )
      bins[i] += weights[i];
  }
  void finalize() noexcept {
    for (auto& bin : bins)
      bin.finalize();
  }
  const Bin& operator[](size_t i) const noexcept { return bins[i]; }
};

struct basic_bin_t { // handle NLO MC multiple entries per event
  double w=0, w2=0, sumw=0;
  int prev_id = -1;
  void operator+=(double weight) noexcept {
    if (prev_id != event_id) {
      w += sumw;
      w2 += sumw*sumw;
      sumw = weight;
      prev_id = event_id;
    } else {
      sumw += weight;
    }
  }
  void finalize() noexcept {
    w += sumw;
    w2 += sumw*sumw;
    sumw = 0;
    prev_id = -1;
  }
};

using namespace ivanp::hist;
using axis_t = variant_axis< uniform_axis<>, cont_axis<> >;
using axes_t = std::vector<std::vector< axis_t >>;
using bin_t = // ***** define bin type *****
  multiweight_tag<
  initial_state_tag<
  photon_cuts_tag<
    basic_bin_t
  >>>;
using hist_t = histogram<
  bin_t,
  axes_spec< const axes_t& >,
  flags_spec< hist_flags::perbin_axes >
>;

template <typename T, bool FirstTag = true>
void save_tags_impl(std::stringstream& ss) {
  if constexpr (!FirstTag) ss << ',';
  ss << '[' << std::quoted(T::name) << ",[";
  { bool first = true;
    for (auto tag : T::tags) {
      if (first) first = false;
      else ss << ',';
      ss << std::quoted(tag);
    }
  }
  ss << "]]";

  using type = std::remove_cvref_t< decltype(std::declval<const T&>()[0]) >;
  if constexpr (!std::is_same_v<type,basic_bin_t>)
    save_tags_impl<type,false>(ss);
}
void save_tags() {
  std::stringstream ss;
  ss << '[';
  save_tags_impl<bin_t>(ss);
  ss << ']';
  TNamed("tags",ss.str().c_str()).Write();
}

TH1D* make_root_hist(const char* name, const uniform_axis<>& ax) {
  return new TH1D(name,"",ax.ndiv(),ax.min(),ax.max());
}
TH1D* make_root_hist(const char* name, const cont_axis<>& ax) {
  return new TH1D(name,"",ax.ndiv(),ax.edges().data());
}

std::string bin_str(const auto& axis, unsigned i) {
  std::stringstream ss;
  ss << "_[" << axis.lower(i) << ',' << axis.upper(i) << ')';
  return std::move(ss).str();
}

// ------------------------------------------------------------------

bool photon_eta_cut(double abs_eta) noexcept {
  return (1.37 < abs_eta && abs_eta < 1.52) || (2.37 < abs_eta);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    cout << "usage: " << argv[0] << " config.json [input.root]\n";
    return 1;
  }

  const auto conf = strcmp(argv[1],"-")
    ? read_json(argv[1])
    : json::parse(std::cin);
  cout << conf/*.dump(2)*/ <<'\n'<< endl;

  // Chain input files
  TChain chain([&]{
    try {
      // get TTree name from config
      const auto& tree = get_str(conf,"input","tree");
      cout << "Specified tree name: " << tree << endl;
      return tree.c_str();
    } catch (...) {
      // get TTree name from ntuple
      // error if more than 1 TTree in file
      cout << "Tree name is not specified\n";
      TFile file(argc>2
        ? argv[2]
        : get_str(conf,"input","files",0).c_str());
      const char* tree_name = nullptr;
      for (TObject* obj : *file.GetListOfKeys()) { // find TTree
        TKey* key = static_cast<TKey*>(obj);
        const TClass* key_class = TClass::GetClass(key->GetClassName(),true);
        if (!key_class) continue;
        if (key_class->InheritsFrom(TTree::Class())) {
          TTree* tree = dynamic_cast<TTree*>(key->ReadObj());
          if (!tree) continue;
          if (!tree_name) tree_name = tree->GetName();
          else if (strcmp(tree->GetName(),tree_name))
            throw std::runtime_error(cat(
              "multiple TTrees in file \"",file.GetName(),"\": \"",
              tree_name,"\" and \"",tree->GetName(),"\""));
        }
      }
      if (tree_name) return tree_name;
      else throw std::runtime_error(cat(
        "no TTree in file \"",file.GetName(),"\""));
    }
  }());
  cout << "Tree name: " << chain.GetName() << endl;
  { // chain input ntuples
    auto add = [&](const char* name){
      cout << name << endl;
      if (!chain.Add(name,0)) {
        cerr << "Failed to add file to TChain\n";
        return true;
      }
      return false;
    };
    if (argc>2) {
      for (int i=2; i<argc; ++i)
        if (add(argv[i])) return 1;
    } else {
      for (const auto& file : get(conf,"input","files"))
        if (add(get_str(file).c_str())) return 1;
    }
  }
  cout << endl;

  // Read branches
  chain.LoadTree(-1); // Loads the first file and prevents a warning
  // https://root-forum.cern.ch/t/using-ttreereader-with-tchain/27279
  TTreeReader reader(&chain);
  branch_reader<int>
    b_id(reader,"id"),
    b_nparticle(reader,"nparticle"),
    b_id1(reader,"id1"),
    b_id2(reader,"id2");
  branch_reader<double[],float[]>
    b_px(reader,"px"),
    b_py(reader,"py"),
    b_pz(reader,"pz"),
    b_E (reader,"E" );
  branch_reader<int[]> b_kf(reader,"kf");
  branch_reader<double> b_weight2(reader,"weight2"); // default weight
  multiweight::tags.push_back("weight2");

  std::optional<branch_reader<int>> b_ncount;
  for (auto* b : *reader.GetTree()->GetListOfBranches()) {
    if (!strcmp(b->GetName(),"ncount")) {
      b_ncount.emplace(reader,"ncount");
      break;
    }
  }

  // Prepare for reweighting
  std::vector<reweighter> reweighters;
  if (conf.contains("reweighting")) {
    const auto& defs = conf["reweighting"];
    reweighters.reserve(defs.size());
    // conversion defined in reweighter_json.hh
    for (reweighter::args_struct def : defs) {
      auto& names = reweighters.emplace_back(reader,def).weights_names();
      multiweight::tags.insert(
        multiweight::tags.end(),names.begin(),names.end());
    }
  }

  // weights vector needs to be resized before histograms are created
  weights.resize( multiweight::tags.size() );
  for (const auto& name : multiweight::tags)
    cout << name << '\n';
  cout << endl;

  // Define axes ----------------------------------------------------
  const auto axes = [axes = [&]()
    -> std::vector<std::tuple<std::regex,axes_t>>
  {
    auto& defs = get(conf,"binning");
    if (defs.is_string())
      return read_json(get_str(defs).c_str());
    else
      return defs;
  }()](const char* name) -> auto& {
    std::cmatch m;
    for (const auto& [r,a] : axes)
      if (std::regex_match(name,m,r)) return a;
    throw std::runtime_error(cat("no axes defined for ",name));
  };

  // Define histograms ----------------------------------------------
  std::vector<std::tuple<const char*,hist_t&>> hists;

  const axes_t Njets_axes = {{ ivanp::hist::uniform_axis(-0.5,4.5,5) }};
  hist_t h_Njets_excl(Njets_axes);
  hists.emplace_back("Njets_excl",h_Njets_excl);
  hist_t h_Njets_incl(Njets_axes);
  hists.emplace_back("Njets_incl",h_Njets_incl);

#define h_(NAME) \
  hist_t h_##NAME(axes(STR(NAME))); \
  hists.emplace_back(STR(NAME),h_##NAME);

  // Histograms of main observables #################################
  // ################################################################

  h_(H_pT)
  h_(j1_pT)

  h_(H_pT__Hj_mass)

  // ################################################################
  // ----------------------------------------------------------------
  // FastJet
  const fastjet::JetDefinition jet_def = get(conf,"jets","algorithm");
  // conversion defined in fastjet_json.hh
  fastjet::ClusterSequence::print_banner(); // get it out of the way
  cout << jet_def.description() << '\n' << endl;

  // Define cuts
  const double
    jet_pt_cut = get_val(30.,conf,"jets","cuts","pt"),
    jet_eta_cut = get_val(4.4,conf,"jets","cuts","eta");
  const unsigned njets_min = get_val(0u,conf,"jets","njets_min");

  TEST(jet_pt_cut)
  TEST(jet_eta_cut)
  TEST(njets_min)
  cout << endl;

  long unsigned Ncount = 0, Nevents = 0, Nentries = chain.GetEntries();
  std::array<long unsigned,2> entries_range { 0, Nentries };
  TEST(Nentries)

  try { // select range of entries
    const auto& ents = get(conf,"input","entries");
    if (ents.is_array()) {
      const auto n = ents.size();
      if (n==1) {
        ents.at(0).get_to(entries_range[1]);
      } else if (n==2) {
        ents.get_to(entries_range);
      } else {
        cerr << "input entries range must be a number or an array of "
          "size 1 or 2\n";
        return 1;
      }
    } else {
      ents.get_to(entries_range[1]);
    }
    if (entries_range[0] > entries_range[1])
      std::swap(entries_range[0],entries_range[1]);
    if (entries_range[1] > Nentries)
      entries_range[1] = Nentries;
    if (entries_range != decltype(entries_range){ 0, Nentries }) {
      Nentries = entries_range[1] - entries_range[0];
      reader.SetEntriesRange(entries_range[0],entries_range[1]);
    }
    cout << "Range of entries: "
      << entries_range[0] << " - " << entries_range[1] << endl;
  } catch (...) { }

  // event containers
  std::vector<fastjet::PseudoJet> partons;
  Higgs2diphoton higgs_decay(
    get_val(Higgs2diphoton::seed_type(0),conf,"photons","higgs_decay_seed"));
  vec4 higgs; // Higgs boson
  std::array<vec4,2> photons;

  // EVENT LOOP =====================================================
  for (ivanp::tcnt cnt(entries_range[0],entries_range[1]); cnt; ++cnt) {
    reader.Next(); // read entry
    const bool new_id = [id=*b_id]{ // check if event id changed
      return (event_id != id) ? ((event_id = id),true) : false;
    }();
    if (new_id) {
      Ncount += (b_ncount ? **b_ncount : 1);
      ++Nevents;
    }

    // read 4-momenta -----------------------------------------------
    partons.clear();
    bool got_higgs = false;
    unsigned nphotons = 0;
    for (unsigned i=0, np = *b_nparticle; i<np; ++i) {
      const auto kf = b_kf[i];
      if (kf == 25) { // Higgs boson
        if (got_higgs) {
          cerr << "Entry " << *cnt << " contains more than 1 Higgs boson\n";
          return 1;
        }
        higgs = { b_px[i],b_py[i],b_pz[i],b_E[i] };
        got_higgs = true;
      } else if (kf == 22) { // photon
        if (nphotons > 1) {
          cerr << "Entry " << *cnt << " contains more than 2 photons\n";
          return 1;
        }
        photons[nphotons] = { b_px[i],b_py[i],b_pz[i],b_E[i] };
        ++nphotons;
      } else {
        partons.emplace_back(b_px[i],b_py[i],b_pz[i],b_E[i]);
        // partons.back().set_user_index(i);
      }
      if ((got_higgs + (nphotons>0)) > 1) {
        cerr << "Entry " << *cnt << " contains unexpected particles\n";
        return 1;
      }
    }
    if (!(got_higgs || (nphotons==2))) {
      cerr << "Entry " << *cnt << " is missing expected particles\n";
      return 1;
    }

    // set weights --------------------------------------------------
    { auto w = weights.begin();
      *w = *b_weight2;
      for (auto& rew : reweighters) {
        rew(); // reweight this event
        for (unsigned i=0, n=rew.nweights(); i<n; ++i)
          *++w = rew[i];
      }
    }

    // tag initial state --------------------------------------------
    initial_state::set(*b_id1,*b_id2);

    // H ??? ???? and photon cuts ---------------------------------------
    if (got_higgs) {
      // reuse the same kinematics if entry is part of the same event
      photons = higgs_decay(higgs,new_id);
    } else {
      higgs = photons[0] + photons[1];
    }
    // sort photons by pT
    auto photon_pt = photons | [](const auto& p){ return p.pt(); };
    if (photon_pt[0] < photon_pt[1]) {
      std::swap(photon_pt[0],photon_pt[1]);
      std::swap(photons[0],photons[1]);
    }
    const auto photon_eta = photons | [](const auto& p){ return p.eta(); };
    const double mH = higgs.m();
    photon_cuts::set(!( // apply photon cuts
      (photon_pt[0] < 0.35*mH) or
      (photon_pt[1] < 0.25*mH) or
      photon_eta_cut(std::abs(photon_eta[0])) or
      photon_eta_cut(std::abs(photon_eta[1]))
    ));

    // Jets ---------------------------------------------------------
    std::vector<vec4> jets = fastjet::ClusterSequence(partons,jet_def)
      .inclusive_jets() // get clustered jets
      | [](const auto& j){ return vec4(j); }; // convert to vec4

    jets.erase( std::remove_if( jets.begin(), jets.end(), // apply jet cuts
      [=](const auto& jet){
        return (jet.pt() < jet_pt_cut)
        or (std::abs(jet.eta()) > jet_eta_cut);
      }), jets.end() );
    std::sort( jets.begin(), jets.end(), // sort by pT
      [](const auto& a, const auto& b){ return ( a.pt() > b.pt() ); });
    const unsigned njets = jets.size(); // number of clustered jets

    // Fill Njets histograms
    h_Njets_excl(njets);
    for (auto nj=njets; ; --nj) {
      h_Njets_incl(nj);
      if (!nj) break;
    }

    if (njets < njets_min) continue; // require minimum number of jets

    // Define observables and fill histograms #######################
    // ##############################################################

    const double H_pT = higgs.pt();
    h_H_pT(H_pT);

    if (njets < 1) continue;

    const double j1_pT = jets[0].pt();
    h_j1_pT(j1_pT);

    const auto Hj = higgs + jets[0];
    const double Hj_mass = Hj.m();
    h_H_pT__Hj_mass(H_pT,Hj_mass);

    // ##############################################################
  } // end event loop
  cout << endl;

  // finalize bins
  for (auto& [name,h] : hists)
    for (auto& bin : h)
      bin.finalize();

  // open output ROOT file
  TFile fout(get_str(conf,"output").c_str(),"recreate");
  fout.SetCompressionAlgorithm(ROOT::kLZMA);
  fout.SetCompressionLevel(9);

  // convert histograms to TH1D
  ivanp::Ycombinator([&](auto f, TDirectory* dir, auto&& get){
    using type = std::remove_cvref_t<
      decltype(get(std::declval<const bin_t&>())) >;
    if constexpr (!std::is_same_v<type,basic_bin_t>) {
      const unsigned n = type::tags.size();
      for (unsigned i=0; i<n; ++i)
        f(
          dir->mkdir(ivanp::cstr(type::tags[i])),
          [i,&get](const auto& bin) -> const auto& { return get(bin)[i]; }
        );
    } else {
      dir->cd();
      double *w, *w2;
      for (const auto& [name,h] : hists) {
        const auto& axes = h.axes();
        const unsigned ndim = axes.size(), ndim1 = ndim ? ndim-1 : 0;
        struct indices {
          unsigned
             i = 0, // bin index in the axes
            ni = 0, // number of bins in current axis
             a = 0, // axis index in this dimension
            na = 0; // number of axes in this dimension
        };
        std::vector<indices> ii(ndim);
        for (unsigned d=ndim; d--; )
          ii[d].na = axes[d].size();
        std::vector<std::string> bins_names(ndim1);

        for (auto bin_it = h.bins().begin();;) { // bin loop
          for (unsigned d=ndim1; ; ) { // label bins
            if (d!=0 && ii[d].i==0) {
              auto& [i,ni,a,na] = ii[--d];
              bins_names[d] = bin_str(axes[d][a],i);
            } else break;
          }
          for (unsigned d=ndim; d--; ) { // increment indices
            auto& [i,ni,a,na] = ii[d];
            if (i==0) {
              const auto& axis = axes[d][a];
              ni = axis.nbins();
              if (d==ndim1) {
                size_t len = strlen(name);
                for (const auto& s : bins_names)
                  len += s.size();
                std::string name2;
                name2.reserve(len);
                name2 += name;
                for (const auto& s : bins_names)
                  name2 += s;

                TH1D* h = std::visit([&](const auto& ax){
                  return make_root_hist(name2.c_str(),ax);
                },*axis);
                h->Sumw2(true);
                w  = h->GetArray();
                w2 = h->GetSumw2()->GetArray();
              }
            }
            if ((++i)>=ni) { // carry over
              i = 0;
              if (d) { // next axis
                if (a+1 < na) ++a;
              } else goto end_bin_loop;
            } else break;
          }

          const auto& bin = get(*bin_it++);
          *w ++ = bin.w;
          *w2++ = bin.w2;
        }
end_bin_loop: ;
      }
    }
  })(
    &fout,
    [](const auto& bin) -> const auto& { return bin; }
  );

  fout.cd();
  { TH1D* N = new TH1D("N","",4,0,4);
    TAxis* a = N->GetXaxis();
    a->SetBinLabel(1,"scale");
    a->SetBinLabel(2,"count");
    a->SetBinLabel(3,"events");
    a->SetBinLabel(4,"entries");
    N->SetBinContent(1,Ncount);
    N->SetBinContent(2,Ncount);
    N->SetBinContent(3,Nevents);
    N->SetBinContent(4,Nentries);
  }
  save_tags();

  // write output ROOT file
  fout.Write(0,TObject::kOverwrite);
  cout << "Output: " << fout.GetName() << endl;
}
