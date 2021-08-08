#include <iostream>
#include <sstream>
#include <vector>

#include <TFile.h>
#include <TKey.h>
#include <TH1.h>

#include "ivanp/string.hh"
#include "ivanp/sqlite.hh"

#define STR1(x) #x
#define STR(x) STR1(x)

#define TEST(var) std::cout << \
  "\033[33m" STR(__LINE__) ": " \
  "\033[36m" #var ":\033[0m " << (var) << std::endl;

using std::cout;
using std::cerr;
using std::endl;
using std::get;
using ivanp::cat;

void split(
  std::string_view s,
  std::string_view d,
  std::vector<std::string_view>& v
) {
  const char* a = s.data();
  for (;;) {
    const bool end = s.empty();
    if (end || s.starts_with(d)) {
      v.emplace_back(a,s.data());
      if (end) break;
      s.remove_prefix(d.size());
      a = s.data();
    } else {
      s.remove_prefix(1);
    }
  }
}
std::vector<std::string_view> split(std::string_view s, std::string_view d) {
  std::vector<std::string_view> v;
  split(s,d,v);
  return v;
}

template <typename T, typename D>
std::string join(const T& xs, const D& d) {
  std::stringstream ss;
  for (size_t i=0, n=xs.size(); i<n; ++i) {
    if (i) ss << d;
    ss << xs[i];
  }
  return std::move(ss).str();
}

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

std::vector<std::vector<double>> binning;

void loop(
  TDirectory* dir,
  const std::vector<std::string_view>& labels,
  ivanp::sqlite::stmt& stmt,
  bool first = true
) {
  for (TObject* key : *dir->GetListOfKeys()) {
    const char* const name = key->GetName();
    const char* const class_name = static_cast<TKey*>(key)->GetClassName();
    TClass* const class_ptr = get_class(class_name);
    if (inherits_from<TDirectory>(class_ptr)) {
      auto labels2 = labels;
      labels2.emplace_back(name);
      loop( read_key<TDirectory>(key), labels2, stmt, false );
    } else if (!first && inherits_from<TH1>(class_ptr)) {
      auto labels2 = labels;
      split(name,"__",labels2);
      for (const auto& x : labels2)
        cout << x << ' ';
      cout << endl;

      TH1* const h = read_key<TH1>(key);

      const int n = h->GetNbinsX() + 1;
      std::vector<double> h_edges(n);
      for (int i=0; i<n; ++i)
        h_edges[i] = h->GetBinLowEdge(i+1);
      int axis = -1, naxes = binning.size();
      for (int i=0; i<naxes; ++i) {
        if (h_edges == binning[i]) {
          axis = i;
          break;
        }
      }
      if (axis<0) {
        binning.emplace_back(std::move(h_edges));
        axis = naxes;
      }

      std::stringstream bins;
      char buf[32];
      for (int i=0; i<=n; ++i) {
        sprintf(buf,",%.6g,%.6g",h->GetBinContent(i),h->GetBinError(i));
        bins << (i ? buf : buf+1);
      }

      stmt.reset().clear();
      int v=0;
      try {
        for (const auto& x : labels2)
          stmt.bind(++v,x);
        stmt.bind(++v,axis);
        stmt.bind(++v,std::move(bins).str());
      } catch (const ivanp::sqlite_error& e) {
        cerr << "\033[31m" "too few labels specified" "\033[0m\n" ;
        throw;
      }
      stmt.step();
    }
  }
}

int main(int argc, char* argv[]) {
  if (argc<3) {
    cout << "usage: " << argv[0] << " output.db [input.root ...] [labels ...]\n";
    return 1;
  }

  int arg_in_end = 2;
  for (; arg_in_end<argc; ++arg_in_end) {
    std::string_view arg = argv[arg_in_end];
    if (!arg.ends_with(".root")) break;
  }
  if (arg_in_end >= argc) {
    cerr << "must specify at least one label\n";
    return 1;
  }

  ivanp::sqlite db(argv[1]);

  { std::stringstream sql;
    sql <<
      "BEGIN;"
      "CREATE TABLE hist (\n";
    for (int i=arg_in_end; i<argc; ++i)
      sql << (i==arg_in_end ? "  " : ", ") << argv[i] << " TEXT\n";
    sql <<
      ", axis INTEGER\n"
      ", bins TEXT\n"
      ");";

    sql << "CREATE TABLE axes (\n"
      "  id INTEGER PRIMARY KEY\n"
      ", edges TEXT\n"
      ");";

    db(std::move(sql).str().c_str());
  }

  std::string vals((argc-arg_in_end+2)*2-1,'?');
  for (size_t i=1; i<vals.size(); i+=2)
    vals[i] = ',';
  auto stmt = db.prepare(cat("INSERT INTO hist VALUES (",vals,")").c_str());

  for (int i=2; i<arg_in_end; ++i) {
    std::string_view arg = argv[i];
    TFile fin(arg.data());
    if (fin.IsZombie()) {
      cerr << "Cannot open file \"" << arg << "\"\n";
      return 1;
    }
    arg.remove_prefix(arg.rfind('/')+1);
    loop( &fin, split({arg.data(),arg.size()-5}, "_"), stmt );
  }

  stmt = db.prepare("INSERT INTO axes VALUES (?,?)");
  for (int i=0, n=binning.size(); i<n; ++i) {
    stmt.reset().clear();
    stmt.bind_all(i,join(binning[i],','));
    stmt.step();
  }

  db("END;");
}
