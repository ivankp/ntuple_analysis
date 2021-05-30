#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <algorithm>
#include <numeric>

#include <TFile.h>
#include <TKey.h>
#include <TChain.h>
#include <TEnv.h>

#include <fastjet/ClusterSequence.hh>

#include <nlohmann/json.hpp>

#include "string.hh"
#include "branch_reader.hh"
#include "reweighter.hh"
#include "reweighter_json.hh"
#include "fastjet_json.hh"
#include "tcnt.hh"

#define STR1(x) #x
#define STR(x) STR1(x)

#define TEST(var) std::cout << \
  "\033[33m" STR(__LINE__) ": " \
  "\033[36m" #var ":\033[0m " << (var) << std::endl;

using std::cout;
using std::cerr;
using std::endl;
using nlohmann::json;
namespace fj = fastjet;
using ivanp::cat; // concatenates strings

template <typename... Ts>
const json& get(const json& j, const Ts&... keys) {
  const json* p = &j;
  ( (p = &p->at(keys)), ... );
  return *p;
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

bool photon_eta_cut(double abs_eta) noexcept {
  return (1.37 < abs_eta && abs_eta < 1.52) || (2.37 < abs_eta);
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cout << "usage: " << argv[0] << " config.json\n";
    return 1;
  }

  const auto conf =
    strcmp(argv[1],"-")
    ? json::parse(std::ifstream(argv[1]))
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
      TFile file(get_str(conf,"input","files",0).c_str());
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
  for (const auto& file : get(conf,"input","files")) {
    const char* name = get_str(file).c_str();
    cout << name << endl;
    if (!chain.Add(name,0)) cerr << "\033[31m"
      "Failed to add file to TChain" "\033[0m\n";
  }
  cout << endl;

  // Read branches
  TTreeReader reader(&chain);
  branch_reader<int>
    b_id(reader,"id"),
    b_nparticle(reader,"nparticle");
  branch_reader<double[],float[]>
    b_px(reader,"px"),
    b_py(reader,"py"),
    b_pz(reader,"pz"),
    b_E (reader,"E" );
  branch_reader<int[]> b_kf(reader,"kf");
  branch_reader<double> b_weight2(reader,"weight2"); // default weight

  std::optional<branch_reader<int>> b_ncount;
  for (auto* b : *reader.GetTree()->GetListOfBranches()) {
    if (!strcmp(b->GetName(),"ncount")) {
      b_ncount.emplace(reader,"ncount");
      break;
    }
  }

  std::vector<std::string_view> weights_names { "weight2" };

  // Prepare for reweighting
  std::vector<reweighter> reweighters;
  { const auto& defs = conf.at("reweight");
    reweighters.reserve(defs.size());
    for (reweighter::args_struct def : defs) {
      // conversion defined in reweighter_json.hh
      auto& names = reweighters.emplace_back(reader,def).weights_names();
      weights_names.insert(weights_names.end(),names.begin(),names.end());
    }
  }
  cout << endl;

  /*
  // weight vector needs to be resized before histograms are created
  bin_t::weight.resize( weights_names.size() );
  for (const auto& name : weights_names)
    cout << name << '\n';
  cout << endl;

  // create histograms ----------------------------------------------
  // hist_t h_total(hist_t::axes_type{});
  hist_t h_total;

  std::map<const char*,hist_t*,chars_less> hists {
    {"total",&h_total}
  };

  std::vector<fj::PseudoJet> partons;
  std::vector<vec4> jets;
  Higgs2diphoton higgs_decay(
    conf.value("higgs_decay_seed",Higgs2diphoton::seed_type(0)));
  Higgs2diphoton::photons_type photons;
  vec4 higgs;
  */

  const fj::JetDefinition jet_def = get(conf,"jets","algorithm");
  // conversion defined in fastjet_json.hh
  fj::ClusterSequence::print_banner(); // get it out of the way
  cout << jet_def.description() << '\n' << endl;

  // Read cuts' definitions
  const double
    jet_pt_cut = get_val(30.,conf,"jets","cuts","pt"),
    jet_eta_cut = get_val(4.4,conf,"jets","cuts","eta");
  const unsigned njets_min = get_val(0u,conf,"jets","njets_min");
  const bool apply_photon_cuts = get_val(false,conf,"photons","cuts");

  TEST(jet_pt_cut)
  TEST(jet_eta_cut)
  TEST(njets_min)
  cout << endl;

  /*
  bin_t::id = -1; // so that first entry has new id
  */
  long unsigned Ncount = 0, Nevents = 0, Nentries = chain.GetEntries();
  TEST(Nentries)

  // EVENT LOOP =====================================================
  for (ivanp::tcnt cnt(Nentries); reader.Next(); ++cnt) {
    /*
    const bool new_id = [id=*_id]{
      return (bin_t::id != id) ? ((bin_t::id = id),true) : false;
    }();
    if (new_id) {
      Ncount += (_ncount ? **_ncount : 1);
      ++Nevents;
    }

    // read 4-momenta -----------------------------------------------
    partons.clear();
    const unsigned np = *_nparticle;
    bool got_higgs = false;
    for (unsigned i=0; i<np; ++i) {
      if (_kf[i] == 25) {
        higgs = { _px[i],_py[i],_pz[i],_E[i] };
        got_higgs = true;
      } else {
        partons.emplace_back(_px[i],_py[i],_pz[i],_E[i]);
      }
    }
    if (!got_higgs) THROW("event without Higgs boson (kf==25)");

    // H -> γγ ----------------------------------------------------
    if (apply_photon_cuts) {
      photons = higgs_decay(higgs,new_id);
      auto A_pT = photons | [](const auto& p){ return p.pt(); };
      if (A_pT[0] < A_pT[1]) {
        std::swap(A_pT[0],A_pT[1]);
        std::swap(photons[0],photons[1]);
      }
      const auto A_eta = photons | [](const auto& p){ return p.eta(); };

      if (
        (A_pT[0] < 0.35*125.) or
        (A_pT[1] < 0.25*125.) or
        photon_eta_cut(std::abs(A_eta[0])) or
        photon_eta_cut(std::abs(A_eta[1]))
      ) continue;
    }

    // Jets ---------------------------------------------------------
    jets = fj::ClusterSequence(partons,jet_def)
          .inclusive_jets() // get clustered jets
          | [](const auto& j){ return vec4(j); };

    jets.erase( std::remove_if( jets.begin(), jets.end(), // apply jet cuts
      [=](const auto& jet){
        return (jet.pt() < jet_pt_cut)
        or (std::abs(jet.eta()) > jet_eta_cut);
      }), jets.end() );
    std::sort( jets.begin(), jets.end(), // sort by pT
      [](const auto& a, const auto& b){ return ( a.pt() > b.pt() ); });
    const unsigned njets = jets.size(); // number of clustered jets

    // set weights --------------------------------------------------
    { auto w = bin_t::weight.begin();
      *w = *_weight2;
      for (auto& rew : reweighters) {
        rew(); // reweight this event
        for (unsigned i=0, n=rew.nweights(); i<n; ++i)
          *++w = rew[i];
      }
    }

    // Observables **************************************************
    if (njets < njets_min) continue; // require minimum number of jets

    h_total(0);
    */
  } // end event loop
  // ================================================================
  cout << endl;

  /*
  // finalize bins
  for (auto& [name,h] : hists)
    for (auto& bin : *h)
      bin.finalize();

  // write output file
  { std::ofstream out(argv[2]);
    json jhists(hists);
    jhists["N"] = {
      {"entries",Nentries},
      {"events",Nevents},
      {"count",Ncount}
    };
    if (ends_with(argv[2],".cbor")) {
      auto cbor = json::to_cbor(jhists);
      out.write(
        reinterpret_cast<const char*>(cbor.data()),
        cbor.size() * sizeof(decltype(cbor[0]))
      );
    } else {
      out << jhists << '\n';
    }
  }
  */
}
