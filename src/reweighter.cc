#include "reweighter.hh"

#include <cmath>
#include <array>
#include <map>

#include <LHAPDF/LHAPDF.h>

#include "branch_reader.hh"

namespace {
template <typename... T> [[gnu::always_inline]]
inline auto sq(T... x) { return ((x*x) + ...); }
}

struct event {
  branch_reader<Int_t> nparticle;
  branch_reader<Double_t[],Float_t[]> px, py, pz, E;
  branch_reader<Int_t[]> kf;
  branch_reader<Double_t> alphas;
  branch_reader<Double_t> weight2;
  branch_reader<Double_t> me_wgt;
  branch_reader<Double_t> me_wgt2;
  branch_reader<Double_t> x1;
  branch_reader<Double_t> x2;
  branch_reader<Double_t> x1p;
  branch_reader<Double_t> x2p;
  branch_reader<Int_t> id1;
  branch_reader<Int_t> id2;
  branch_reader<Double_t> fac_scale;
  branch_reader<Double_t> ren_scale;
  branch_reader<Double_t[]> usr_wgts;
  branch_reader<Char_t> alphasPower;
  branch_reader<Char_t[]> part;

  event(TTreeReader& reader)
  : nparticle(reader,"nparticle"),
    px(reader,"px"), py(reader,"py"), pz(reader,"pz"), E(reader,"E"),
    kf(reader,"kf"),
    alphas(reader,"alphas"),
    weight2(reader,"weight2"),
    me_wgt(reader,"me_wgt.me_wtg"),
    me_wgt2(reader,"me_wgt2.me_wtg2"),
    x1(reader,"x1"),
    x2(reader,"x2"),
    x1p(reader,"x1p"),
    x2p(reader,"x2p"),
    id1(reader,"id1"),
    id2(reader,"id2"),
    fac_scale(reader,"fac_scale"),
    ren_scale(reader,"ren_scale"),
    usr_wgts(reader,"usr_wgts"),
    alphasPower(reader,"alphasPower"),
    part(reader,"part")
  { }
};

std::vector<std::unique_ptr<LHAPDF::PDF>>
make_pdfs(const std::string& name, bool variations) {
  if (variations) {
    auto pdfs = LHAPDF::mkPDFs(name);
    return { pdfs.begin(), pdfs.end() };
  } else {
    std::vector<std::unique_ptr<LHAPDF::PDF>> pdfs;
    pdfs.emplace_back( LHAPDF::mkPDF(name) );
    return pdfs;
  }
}

struct reweighter_impl: event {
  const reweighter::args_struct args;

  std::vector<std::unique_ptr<LHAPDF::PDF>> pdfs;
  LHAPDF::PDF *pdf = nullptr;

  using scale_function = double(*)(event&);
  static std::map<std::string, scale_function> scale_functions;
  static scale_function& get_scale_fcn(const std::string& name) {
    try {
      return scale_functions.at(name);
    } catch (...) {
      throw std::runtime_error(ivanp::cat(
        "cannot find scale function \"",name,"\""));
    }
  }
  scale_function& scale_f;

  double scale_base_value;

  // TODO: these are not correctly indexed
  struct ren_vars_struct { double k, ar, w0; };
  struct fac_vars_struct { double k,  m, ff; };
  std::vector<ren_vars_struct> ren_vars;
  std::vector<fac_vars_struct> fac_vars;
  std::vector<double> weights;
  std::vector<std::string> weights_names;

  std::string make_weight_name(
    const std::string& scale, unsigned ki, unsigned pdfi
  ) {
    auto* pdf = pdfs[pdfi].get();
    std::stringstream ss;
    ss << scale << ' ' << pdf->set().name() << ':' << pdf->memberID();
    if (args.Ki[ki].ren) ss << " ren:" << args.Kr[*args.Ki[ki].ren];
    if (args.Ki[ki].fac) ss << " fac:" << args.Kf[*args.Ki[ki].fac];
    return ss.str();
  }

  reweighter_impl(TTreeReader& reader, reweighter::args_struct _args)
  : event(reader),
    args(std::move(_args)),
    pdfs(make_pdfs(args.pdf,args.pdf_var)),
    scale_f(get_scale_fcn(args.scale)),
    scale_base_value(),
    ren_vars(args.Kr.size()), fac_vars(args.Kf.size()),
    weights(args.Ki.size()+pdfs.size()-1), weights_names()
  {
    for (unsigned i=0; i<args.Kr.size(); ++i) ren_vars[i].k = args.Kr[i];
    for (unsigned i=0; i<args.Kf.size(); ++i) fac_vars[i].k = args.Kf[i];

    weights_names.reserve(weights.size());
    for (unsigned i=0; i<args.Ki.size(); ++i) // scale variations
      weights_names.emplace_back(make_weight_name(args.scale,i,0));
    for (unsigned i=1; i<pdfs.size(); ++i) // pdf variations
      weights_names.emplace_back(make_weight_name(args.scale,0,i));
  }

  struct fac_calc_struct {
    LHAPDF::PDF *pdf;
    double muF;
    std::array<int,2> id;
    std::array<double,2> x, xp;

    static constexpr std::array<int,10> quarks {
      1,-1, 2,-2, 3,-3, 4,-4, 5,-5
    };

    double fxQ(int id, double x1, double x2) const {
      return pdf->xfxQ(id, x1, muF)/x2;
    }
    double fxQ(int id, double x) const {
      return pdf->xfxQ(id, x, muF)/x;
    }
    double f(unsigned r) const {
      return fxQ(id[r],x[r]);
    }
    double f1(unsigned r) const {
      const double _x = x[r];
      if (id[r] != 21) return fxQ(id[r],_x);
      else {
        double f = 0.;
        for (int q : quarks) f += fxQ(q,_x);
        return f;
      }
    };
    double f2(unsigned r) const {
      const double _x  = x[r];
      const double _xp = _x/xp[r];
      if (id[r] != 21) return fxQ(id[r],_xp,_x);
      else {
        double f = 0.;
        for (int q : quarks) f += fxQ(q,_xp,_x);
        return f;
      }
    };
    double f3(unsigned r) const {
      return fxQ(21,x[r]);
    }
    double f4(unsigned r) const {
      return fxQ(21,x[r]/xp[r],x[r]);
    }
  } fc;

  void fac_calc(fac_vars_struct& vars) {
    fc.muF = scale_base_value * vars.k;
    fc.id  = { *id1, *id2 };
    fc.x   = { * x1, * x2 };

    const double f[2] = { fc.f(0), fc.f(1) };
    vars.ff = f[0]*f[1];

    if (part[0]=='I') {
      fc.xp = { *x1p, *x2p };
      const double lf = 2.*std::log(fc.muF/(*fac_scale));
      double w[8];
      for (int i=0; i<8; ++i) w[i] = usr_wgts[i+2] + usr_wgts[i+10]*lf;

      vars.m = ( fc.f1(0)*w[0] + fc.f2(0)*w[1]
               + fc.f3(0)*w[2] + fc.f4(0)*w[3] )*f[1]
             + ( fc.f1(1)*w[4] + fc.f2(1)*w[5]
               + fc.f3(1)*w[6] + fc.f4(1)*w[7] )*f[0];
    } else vars.m = 0.;
  }

  void ren_calc(ren_vars_struct& vars) {
    const double muR = scale_base_value * vars.k;

    vars.ar = std::pow(pdf->alphasQ(muR)/(*alphas), (*alphasPower));

    if (part[0]=='V' || part[0]=='I') {
      const double lr = 2.*std::log(muR/(*ren_scale));
      vars.w0 = (*me_wgt) + lr*usr_wgts[0] + 0.5*lr*lr*usr_wgts[1];
    } else {
      vars.w0 = *me_wgt2;
    }
  }

  double combine(const reweighter::ren_fac<unsigned>& ki) {
    double w;

    if (ki.fac) {
      const auto& fac = fac_vars[*ki.fac];
      w = fac.m;
      if (ki.ren) {
        const auto& ren = ren_vars[*ki.ren];
        w += ren.w0 * fac.ff;
      } else {
        w += (*me_wgt2) * fac.ff;
      }
    } else {
      w = *weight2;
    }
    if (ki.ren) {
      const auto& ren = ren_vars[*ki.ren];
      w *= ren.ar;
    }

    return w;
  }

  void operator()() {
    scale_base_value = scale_f(*this);

    pdf = fc.pdf = pdfs[0].get();
    for (auto& vars : ren_vars) ren_calc(vars);
    for (auto& vars : fac_vars) fac_calc(vars);

    // scale variations
    unsigned wi = 0;
    for (; wi<args.Ki.size(); ++wi)
      weights[wi] = combine(args.Ki[wi]);

    // pdf variations
    for (unsigned i=1; i<pdfs.size(); ++i, ++wi) {
      pdf = fc.pdf = pdfs[i].get();
      ren_calc(ren_vars[0]);
      fac_calc(fac_vars[0]);
      weights[wi] = combine(args.Ki[0]);
    }
  }
};
constexpr std::array<int,10> reweighter_impl::fac_calc_struct::quarks;

reweighter::reweighter(TTreeReader& reader, args_struct args)
: impl(new reweighter_impl(reader, std::move(args))) { }
reweighter::reweighter(reweighter&& o): impl(o.impl) { o.impl = nullptr; }
reweighter::~reweighter() { delete impl; }

void reweighter::args_struct::add_scale(
  const ren_fac<double>& k
) {
  ren_fac<unsigned> ki { };
  auto fcn = [&](auto& K, auto t) {
    if (t(k)) {
      auto it = std::find(K.begin(), K.end(), *t(k));
      if (it==K.end()) {
        t(ki) = K.size();
        K.push_back(*t(k));
      } else {
        t(ki) = it - K.begin();
      }
    }
  };
  fcn(Kr, [](auto& x) -> auto& { return x.ren; });
  fcn(Kf, [](auto& x) -> auto& { return x.fac; });
  Ki.push_back(ki);
}

void reweighter::operator()() { (*impl)(); }
unsigned reweighter::nweights() const {
  return impl->weights.size();
}
double reweighter::operator[](unsigned i) const {
  return impl->weights[i];
}
const std::vector<std::string>& reweighter::weights_names() const {
  return impl->weights_names;
}

decltype(reweighter_impl::scale_functions)
reweighter_impl::scale_functions {
  {"HT1", [](event& e){
    double HT = 0.;
    for (int i=0, n=*e.nparticle; i<n; ++i)
      HT += std::sqrt( e.kf[i]==25
          ? sq(e.E[i])-sq(e.pz[i]) // ET for Higgs
          : sq(e.px[i],e.py[i]) ); // pT
    return 0.5*HT;
  }},
  {"HT2", [](event& e){
    double HT = 0., mH = 0;
    for (int i=0, n=*e.nparticle; i<n; ++i) {
      HT += std::sqrt( sq(e.px[i],e.py[i]) ); // pT
      if (e.kf[i]==25)
        mH = std::sqrt( sq(e.E[i]) - sq(e.px[i],e.py[i],e.pz[i]) );
    }
    return mH + 0.5*HT;
  }},
  {"HT", [](event& e){
    double HT = 0.;
    for (int i=0, n=*e.nparticle; i<n; ++i)
      HT += std::sqrt( sq(e.px[i],e.py[i]) ); // pT
    return HT;
  }}
};

