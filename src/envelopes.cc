#include <array>
#include <tuple>
#include <map>
#include <unordered_map>
#include <regex>
#include <stdexcept>

#include <TFile.h>
#include <TKey.h>
#include <TH1.h>

#include <LHAPDF/LHAPDF.h>

#include "ivanp/string.hh"
#include "ivanp/cont/map.hh"

#define STR1(x) #x
#define STR(x) STR1(x)

#define TEST(var) std::cout << \
  "\033[33m" STR(__LINE__) ": " \
  "\033[36m" #var ":\033[0m " << (var) << std::endl;

using std::cout;
using std::cerr;
using std::endl;
using ivanp::cat;

using namespace ivanp::cont::ops::map;

std::map<const char*,TClass*,ivanp::chars_less> classes;
TClass* get_class(const char* name) {
  auto it = classes.find(name);
  if (it != classes.end()) return it->second;
  TClass* const class_ptr = TClass::GetClass(name,true,true);
  if (!class_ptr) throw std::runtime_error(cat(
    "No TClass found for \"",name,"\""));
  return classes[class_ptr->GetName()] = class_ptr;
}

std::unordered_map<std::string,LHAPDF::PDFSet> pdf_sets;
const LHAPDF::PDFSet* get_pdf_set(const std::string& name) {
  return &pdf_sets.try_emplace(name,name).first->second;
}

template <typename T>
bool inherits_from(TClass* c) {
  return c->InheritsFrom(T::Class());
}
template <typename T = TObject>
T* read_key(TObject* key) {
  return static_cast<T*>(static_cast<TKey*>(key)->ReadObj());
}
template <typename T>
T get_obj(TDirectory* dir, const char* name) {
  if constexpr (std::is_pointer_v<T>)
    return dynamic_cast<T>(dir->Get(name));
  else
    return dynamic_cast<T&>(*dir->Get(name));
}

void scale_unc(const std::vector<TH1*>& vars, TH1* u, TH1* d) {
  const auto n = u->GetNcells();
  for (TH1* h : vars)
    for (auto i = n; i--; ) {
      const auto v = h->GetBinContent(i);
      if (v > u->GetBinContent(i)) u->SetBinContent(i,v);
      if (v < d->GetBinContent(i)) d->SetBinContent(i,v);
    }
}
void pdf_unc(
  const LHAPDF::PDFSet* pdf_set,
  const std::vector<TH1*>& vars,
  TH1* u,
  TH1* d
) {
  const auto nbins = u->GetNcells();
  const auto nvars = vars.size();
  std::vector<double> values(nvars);
  LHAPDF::PDFUncertainty unc;
  for (auto i = nbins; i--; ) {
    for (auto j = nvars; j--; )
      values[j] = vars[j]->GetBinContent(i);
    pdf_set->uncertainty(unc,values);
    const double v = values[0];
    u->SetBinContent(i,v+unc.errplus);
    d->SetBinContent(i,v-unc.errminus);
  }
}

void loop_envelopes(
  const std::array<TDirectory*,3>& out,
  TDirectory* nom,
  const std::vector<TDirectory*>& vars,
  const LHAPDF::PDFSet* pdf_set
) {
  for (TObject* key : *nom->GetListOfKeys()) {
    const char* const name = key->GetName();
    const char* const class_name = static_cast<TKey*>(key)->GetClassName();
    TClass* const class_ptr = get_class(class_name);
    if (inherits_from<TDirectory>(class_ptr)) {
      loop_envelopes(
        out | [&](TDirectory* d){ return d ? d->mkdir(name) : nullptr; },
        read_key<TDirectory>(key),
        vars | [&](TDirectory* d){ return &get_obj<TDirectory&>(d,name); },
        pdf_set
      );
    } else if (inherits_from<TH1>(class_ptr)) {
      TH1* hs[3];
      TH1* h = read_key<TH1>(key);
      hs[0] = h; // always needed for pdf uncertainties
      for (unsigned i=(out[0]?0:1); i<3; ++i) {
        out[i]->cd();
        hs[i] = h = static_cast<TH1*>(h->Clone());
        if (i==1) h->Sumw2(false); // stat. unc. only for nominal
      }
      if (pdf_set) {
        const size_t n = vars.size();
        std::vector<TH1*> pdf_vars(n+1);
        pdf_vars[0] = hs[0];
        for (size_t i=0; i<n; ++i)
          pdf_vars[i+1] = &get_obj<TH1&>(vars[i],name);
        pdf_unc(pdf_set, pdf_vars, hs[1], hs[2]);
      } else {
        scale_unc(
          vars | [&](TDirectory* d){ return &get_obj<TH1&>(d,name); },
          hs[1], hs[2]
        );
      }
    }
  }
}

void loop_clone(TDirectory* in, TDirectory* out) {
  for (TObject* key : *in->GetListOfKeys()) {
    const char* const name = key->GetName();
    const char* const class_name = static_cast<TKey*>(key)->GetClassName();
    TClass* const class_ptr = get_class(class_name);
    if (inherits_from<TDirectory>(class_ptr)) {
      loop_clone( read_key<TDirectory>(key), out->mkdir(name) );
    } else {
      out->WriteObject(read_key(key),name);
    }
  }
}

struct variation {
  TDirectory* nom;
  std::map<std::array<double,2>,TDirectory*> scale;
  std::map<int,TDirectory*> pdf;
};

std::regex r(R"((.*\s)([^\s]+):(\d+) ren:([\d.]+) fac:([\d.]+)(?:\s+(.*))?)");

int main(int argc, char* argv[]) {
  if (argc!=3) {
    cout << "usage: " << argv[0] << " input.root output.root\n";
    return 1;
  }

  std::map<std::array<std::string,3>,variation> variations;

  TFile fin(argv[1]);
  if (fin.IsZombie()) return 1;

  TFile fout(argv[2],"recreate");
  if (fout.IsZombie()) return 1;
  fout.SetCompressionAlgorithm(ROOT::kLZMA);
  fout.SetCompressionLevel(9);

  for (TObject* key : *fin.GetListOfKeys()) {
    const char* const name = key->GetName();
    const char* const class_name = static_cast<TKey*>(key)->GetClassName();
    TClass* const class_ptr = get_class(class_name);
    if (inherits_from<TDirectory>(class_ptr)) {
      std::cmatch m;
      if (std::regex_match(name,m,r)) {
        const int    pdf = std::stoi(m[3]);
        const double ren = std::stod(m[4]);
        const double fac = std::stod(m[5]);
        const bool   nom_pdf   = (pdf == 0),
                     nom_scale = (ren == 1) && (fac == 1);
        auto& v = variations[{m.str(1),m.str(2),m.str(6)}];
        TDirectory* dir = read_key<TDirectory>(key);
        if (!nom_pdf && !nom_scale) {
          cerr << "unexpected variation " << m[2] << ':' << m[3]
            << " ren:" << m[4] << " fac:" << m[5] << endl;
        } else {
          auto& d = ( nom_pdf && nom_scale
            ? v.nom
            : ( nom_pdf ? v.scale[{ren,fac}] : v.pdf[pdf] ) );
          if (d) {
            cerr << "duplicate " << name << '\n';
            return 1;
          }
          d = dir;
        }
      } else {
        loop_clone( read_key<TDirectory>(key), fout.mkdir(name) );
      }
    } else if (!strcmp(name,"tags")) {
    } else {
      fout.WriteObject(read_key(key),name);
    }
  }
  for (const auto& [lbl,v] : variations) {
    const auto name = cat(lbl[0],lbl[1]," "+!lbl[2].size(),lbl[2]);
    cout << name << endl;
    if (!v.scale.empty()) { // scale variations
      loop_envelopes(
        { fout.mkdir(name.c_str()),
          fout.mkdir((name+" scale_up").c_str()),
          fout.mkdir((name+" scale_down").c_str())
        },
        v.nom,
        v.scale | [](const auto& x){ return x.second; },
        nullptr
      );
    }
    if (!v.pdf.empty()) { // PDF variations
      const auto* pdf_set = get_pdf_set(lbl[1]);
      cout << pdf_set->description() << endl;
      loop_envelopes(
        { v.scale.empty() ? fout.mkdir(name.c_str()) : nullptr,
          fout.mkdir((name+" pdf_up").c_str()),
          fout.mkdir((name+" pdf_down").c_str())
        },
        v.nom,
        v.pdf | [](const auto& x){ return x.second; },
        pdf_set
      );
    }
  }

  fout.Write(0,TObject::kOverwrite);
}
