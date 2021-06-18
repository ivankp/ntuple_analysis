#include <iostream>
#include <stdexcept>
#include <map>

#include <unistd.h>

#include <TFile.h>
#include <TKey.h>
#include <TH1.h>

#include "ivanp/string.hh"

using std::cout;
using std::cerr;
using std::endl;
using ivanp::cat;

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
template <typename T>
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

void loop_add(TDirectory* out, TDirectory* in, bool first) {
  for (TObject* key : *in->GetListOfKeys()) {
    const char* const name = key->GetName();
    const char* const class_name = static_cast<TKey*>(key)->GetClassName();
    TClass* const class_ptr = get_class(class_name);
    if (inherits_from<TDirectory>(class_ptr)) {
      loop_add(
        first ? out->mkdir(name) : &get_obj<TDirectory&>(out,name),
        read_key<TDirectory>(key),
        first
      );
    } else if (inherits_from<TH1>(class_ptr)) {
      TH1* h = read_key<TH1>(key);
      if (first) {
        out->cd();
        h->Clone();
      } else {
        get_obj<TH1&>(out,name).Add(h);
      }
    }
  }
}

template <bool first=true>
void loop_xsec(TDirectory* dir, double factor) {
  for (TObject* obj : *dir->GetList()) {
    TClass* const class_ptr = get_class(obj->ClassName());
    if (inherits_from<TDirectory>(class_ptr)) {
      loop_xsec<false>( static_cast<TDirectory*>(obj), factor );
    } else if (inherits_from<TH1>(class_ptr)) {
      if constexpr (first)
        if (!strcmp(obj->GetName(),"N")) continue;
      static_cast<TH1*>(obj)->Scale(factor,"width");
    }
  }
}

bool opt_x = false, opt_e = false;
#define TOGGLE(x) x = !x

void print_usage(const char* prog) {
  cout << "usage: " << prog << " [options ...] output.root input1.root [...]\n"
    "  -e           scale and pdf envelopes\n"
    "  -x           convert weight to cross section\n"
    "               and divide by bin width\n"
    "  -h, --help   display this help text and exit\n";
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }
  for (int i=1; i<argc; ++i) { // long options
    const char* arg = argv[i];
    if (*(arg++)=='-' && *(arg++)=='-') {
      if (!strcmp(arg,"help")) {
        print_usage(argv[0]);
        return 0;
      }
    }
  }
  for (int o; (o = getopt(argc,argv,"hxe")) != -1; ) { // short options
    switch (o) {
      case 'h': print_usage(argv[0]); return 0;
      case 'x': TOGGLE(opt_x); break;
      case 'e': TOGGLE(opt_e); break;
      default : return 1;
    }
  }

  cout << "output: " << argv[optind] << endl;
  TFile fout(argv[optind],"recreate"); // open output file
  if (fout.IsZombie()) return 1;
  fout.SetCompressionAlgorithm(ROOT::kLZMA);
  fout.SetCompressionLevel(9);
  ++optind;

  TObject* tags1 { };
  for (int i=optind; i<argc; ++i) { // loop over input files
    const bool first = (i==optind);
    cout << "input: " << argv[i] << endl;
    TFile fin(argv[i]);
    if (fin.IsZombie()) return 1;

    TObject* tags = fin.Get("tags");
    if (first && tags) {
      // cout << tags->GetTitle() << endl;
      tags1 = tags->Clone();
    } else if (
      ( (!tags) != (!tags1) ) ||
      ( tags && strcmp(tags->GetTitle(),tags1->GetTitle()) )
    ) {
      cerr << "differing sets of histogram tags\n";
      return 1;
    }

    loop_add(&fout,&fin,first);
  }

  if (opt_x) { // convert weight to cross section and divide by bin width
    TH1* N = get_obj<TH1*>(&fout,"N");
    if (N) {
      const double
        scale = N->GetBinContent(1),
        count = N->GetBinContent(2);
      if (scale==count) {
        cout << "scaling to cross section, 1/" << scale << endl;
        loop_xsec(&fout,1./scale);
        N->SetBinContent(1,1);
      } else {
        cerr << "input histograms appear to have already been scaled" << endl;
      }
    } else {
      cerr << "cannot scale to cross section without \"N\" histogram" << endl;
    }
  }

  fout.cd();
  if (tags1) tags1->Write();

  fout.Write(0,TObject::kOverwrite);
}
