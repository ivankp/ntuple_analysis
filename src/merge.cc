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

template <typename F>
void loop(TDirectory* out, TDirectory* in, bool first, F&& f) {
  for (TObject* key : *in->GetListOfKeys()) {
    const char* const name = key->GetName();
    const char* const class_name = static_cast<TKey*>(key)->GetClassName();
    TClass* const class_ptr = get_class(class_name);
    if (inherits_from<TDirectory>(class_ptr))
      loop(
        first
        ? out->mkdir(name)
        : &dynamic_cast<TDirectory&>(*out->Get(name)),
        static_cast<TDirectory*>(static_cast<TKey*>(key)->ReadObj()),
        first,
        f
      );
    else
      f(out,static_cast<TKey*>(key),class_ptr,first);
  }
}

bool opt_x = false, opt_e = false;
#define TOGGLE(x) x = !x

void print_usage(const char* prog) {
  cout << "usage: " << prog << " [options ...] output.root [input.root ...]\n"
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
  for (int i=1; i<argc; ++i) {
    const char* arg = argv[i];
    if (*(arg++)=='-' && *(arg++)=='-') {
      if (!strcmp(arg,"help")) {
        print_usage(argv[0]);
        return 0;
      }
    }
  }
  for (int o; (o = getopt(argc,argv,"hxe")) != -1; ) {
    switch (o) {
      case 'h': print_usage(argv[0]); return 0;
      case 'x': TOGGLE(opt_x); break;
      case 'e': TOGGLE(opt_e); break;
      default : return 1;
    }
  }

  cout << "output: " << argv[optind] << endl;
  TFile fout(argv[optind],"recreate");
  if (fout.IsZombie()) return 1;
  fout.SetCompressionAlgorithm(ROOT::kLZMA);
  fout.SetCompressionLevel(9);
  ++optind;

  TObject* tags1 { };
  for (int i=optind; i<argc; ++i) {
    const bool first = (i==optind);
    cout << "input: " << argv[i] << endl;
    TFile fin(argv[i]);
    if (fin.IsZombie()) return 1;

    TObject* tags = fin.Get("tags");
    if (first && tags) {
      cout << tags->GetTitle() << endl;
      tags1 = tags->Clone();
    } else if (
      ( (!tags) != (!tags1) ) ||
      ( tags && strcmp(tags->GetTitle(),tags1->GetTitle()) )
    ) {
      cerr << "differing sets of histogram tags\n";
      return 1;
    }

    loop(&fout,&fin,first,[](
      TDirectory* out, TKey* key, TClass* class_ptr, bool first
    ){
      if (inherits_from<TH1>(class_ptr)) {
        auto* h  = static_cast<TH1*>(key->ReadObj());
        if (first) {
          out->cd();
          h->Clone();
        } else {
          dynamic_cast<TH1&>(*out->Get(key->GetName())).Add(h);
        }
      }
    });
  }
  fout.cd();
  if (tags1) tags1->Write();

  fout.Write(0,TObject::kOverwrite);
}
