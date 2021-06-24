#include <array>
#include <tuple>
#include <map>
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

// template <typename H>
// bool scale_var_impl(const std::array<H*,3>& h) {
//   if (!(h[0] && h[1] && h[2])) return false;
//   const auto w = h | [](auto* h){
//     return std::make_tuple(h->GetArray(), h->GetSumw2()->GetArray());
//   };
//   for (auto i = h[0]->GetNcells(); i--; ) {
//     const auto v = std::get<0>(w[0])[i];
//     auto& u = std::get<0>(w[1])[i];
//     auto& d = std::get<0>(w[2])[i];
//     if (v > u) u = v;
//     if (v < d) d = v;
//   }
//   return true;
// }
// void scale_var(const std::array<TH1*,3>& h) {
//   if (![&]<typename... H>(ivanp::type_sequence<H...>){
//     return ( scale_var_impl( h | [](auto* h){ return dynamic_cast<H*>(h); } )
//       || ... );
//   }(ivanp::type_sequence<
//     TH1D, TH1F
//   >{})) throw std::runtime_error(cat(
//     h[0]->GetName()," has unexpected type ",h[0]->ClassName()));
// }

void scale_var(TH1* h, TH1* u, TH1* d) {
  for (auto i = h->GetNcells(); i--; ) {
    const auto v = h->GetBinContent(i);
    if (v > u->GetBinContent(i)) u->SetBinContent(i,v);
    if (v < d->GetBinContent(i)) d->SetBinContent(i,v);
  }
}

void loop_envelopes(
  const std::array<TDirectory*,3>& out,
  TDirectory* nom,
  const std::vector<TDirectory*>& vars,
  bool pdf
) {
  for (TObject* key : *nom->GetListOfKeys()) {
    const char* const name = key->GetName();
    const char* const class_name = static_cast<TKey*>(key)->GetClassName();
    TClass* const class_ptr = get_class(class_name);
    if (inherits_from<TDirectory>(class_ptr)) {
      loop_envelopes(
        out | [name](TDirectory* d){ return d->mkdir(name); },
        read_key<TDirectory>(key),
        vars | [name](TDirectory* d){ return &get_obj<TDirectory&>(d,name); },
        pdf
      );
    } else if (inherits_from<TH1>(class_ptr)) {
      auto h = out | [h=read_key<TH1>(key),i=-2](TDirectory* d) mutable {
        d->cd();
        h = static_cast<TH1*>(h->Clone());
        if (!++i) h->Sumw2(false);
        return h;
      };
      for (TDirectory* d : vars) {
        h[0] = &get_obj<TH1&>(d,name);
        if (pdf) {
        } else {
          // std::apply(scale_var,h);
          scale_var(h[0],h[1],h[2]);
        }
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
  for (const auto& [name,v] : variations) {
    const auto fullname = cat(name[0],name[1]," "+!name[2].size(),name[2]);
    cout << fullname << endl;
    // for (const auto& [mu,d] : v.scale)
    //   cout << "  " << mu[0] << ' ' << mu[1]  << ": " << d->GetName() << endl;
    // for (const auto& [i,d] : v.pdf)
    //   cout << "  " << i << ": " << d->GetName() << endl;

    loop_envelopes( // scale variations
      { fout.mkdir(fullname.c_str()),
        fout.mkdir(cat(fullname," scale_up").c_str()),
        fout.mkdir(cat(fullname," scale_down").c_str())
      },
      v.nom,
      v.scale | [](const auto& x){ return x.second; },
      false
    );
  }

  fout.Write(0,TObject::kOverwrite);
}
