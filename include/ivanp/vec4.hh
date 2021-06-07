#ifndef IVANP_VEC4_HH
#define IVANP_VEC4_HH

#include <cmath>
#include <algorithm>
#include <limits>

namespace ivanp {

namespace num {
inline constexpr double pi = M_PI;
inline constexpr double twopi = pi*2;
}

template <typename... T>
[[ gnu::always_inline ]]
constexpr auto sq(const T&... x) noexcept { return (... + (x*x)); }

struct vec3;
struct vec4;

} // end namespace ivanp

namespace std {

template <> struct tuple_size<ivanp::vec3>: integral_constant<size_t,3> { };
template <> struct tuple_size<ivanp::vec4>: integral_constant<size_t,4> { };

template <size_t I>
struct tuple_element<I,ivanp::vec3> { using type = double; };
template <size_t I>
struct tuple_element<I,ivanp::vec4> { using type = double; };

} // end namespace std

namespace ivanp {

struct vec3 {
  double v[3];

  struct XYZ_t { } static constexpr XYZ { };
  struct PtEtaPhi_t { } static constexpr PtEtaPhi { };

  constexpr vec3() noexcept: v{} { }
  constexpr vec3(double x, double y, double z) noexcept: v{x,y,z} { }
  constexpr vec3(double x, double y, double z, XYZ_t) noexcept: v{x,y,z} { }
  vec3(double pt, double eta, double phi, PtEtaPhi_t) noexcept {
    pt = std::abs(pt);
    v[0] = pt*std::cos(phi);
    v[1] = pt*std::sin(phi);
    v[2] = pt*std::sinh(eta);
  }
  template <typename U>
  vec3(const U& p) noexcept: vec3(p[0], p[1], p[2]) { }
  template <typename U>
  vec3(const U& p, XYZ_t) noexcept: vec3(p[0], p[1], p[2], XYZ) { }
  template <typename U>
  vec3(const U& p, PtEtaPhi_t) noexcept: vec3(p[0], p[1], p[2], PtEtaPhi) { }

  constexpr double& operator[](unsigned i) noexcept { return v[i]; }
  constexpr const double& operator[](unsigned i) const noexcept { return v[i]; }

  constexpr double x() const noexcept { return v[0]; }
  constexpr double y() const noexcept { return v[1]; }
  constexpr double z() const noexcept { return v[2]; }

  constexpr double px() const noexcept { return v[0]; }
  constexpr double py() const noexcept { return v[1]; }
  constexpr double pz() const noexcept { return v[2]; }

  constexpr double pt2() const noexcept { return sq(v[0],v[1]); }
  double pt() const noexcept { return std::sqrt(pt2()); }
  constexpr double norm2() const noexcept { return sq(v[0],v[1],v[2]); }
  double norm() const noexcept { return std::sqrt(norm2()); }
  double cos_theta() const noexcept {
    const double a = norm();
    return a != 0 ? v[2]/a : 1;
  }
  double eta() const noexcept {
    const double ct = cos_theta();
    if (__builtin_expect(std::abs(ct) < 1, 1))
      return -0.5*std::log((1-ct)/(1+ct));
    if (v[2] == 0) return 0;
    if (v[2] >  0) return  std::numeric_limits<double>::infinity();
    else           return -std::numeric_limits<double>::infinity();
  }
  double phi() const noexcept { return std::atan2(v[1],v[0]); }

  vec3& normalize(double n=1) noexcept {
    const double c = norm();
    if (__builtin_expect(c != 0,1 )) (*this) *= n/c;
    return *this;
  }
  vec3 normalized(double n=1) const noexcept {
    return vec3(*this).normalize(n);
  }

  vec3& rotate_u_z(const vec3& u) noexcept {
    double up = sq(u[0],u[1]);
    if (__builtin_expect(up, 1)) {
      up = std::sqrt(up);
      const auto [px,py,pz] = *this;
      (*this) = {
        (u[0]*u[2]*px - u[1]*py + u[0]*up*pz)/up,
        (u[1]*u[2]*px + u[0]*py + u[1]*up*pz)/up,
        (u[2]*u[2]*px -      px + u[2]*up*pz)/up
      };
    } else if (u[2] < 0) { v[0] = -v[0]; v[2] = -v[2]; }
    return *this;
  }

  constexpr vec3& operator+=(const vec3& r) noexcept {
    v[0] += r[0];
    v[1] += r[1];
    v[2] += r[2];
    return *this;
  }
  constexpr vec3& operator-=(const vec3& r) noexcept {
    v[0] -= r[0];
    v[1] -= r[1];
    v[2] -= r[2];
    return *this;
  }
  constexpr vec3& operator*=(double r) noexcept {
    v[0] *= r;
    v[1] *= r;
    v[2] *= r;
    return *this;
  }
  constexpr vec3& operator/=(double r) noexcept {
    v[0] /= r;
    v[1] /= r;
    v[2] /= r;
    return *this;
  }
  constexpr vec3 operator-() const noexcept {
    return { -v[0], -v[1], -v[2] };
  }

  template <size_t I>
  friend constexpr const double& get(const vec3& v) noexcept {
    return v[I];
  }
};

constexpr vec3 operator+(const vec3& a, const vec3& b) noexcept
{ return { a[0]+b[0], a[1]+b[1], a[2]+b[2] }; }

constexpr vec3 operator-(const vec3& a, const vec3& b) noexcept
{ return { a[0]-b[0], a[1]-b[1], a[2]-b[2] }; }

constexpr double operator*(const vec3& a, const vec3& b) noexcept
{ return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }

constexpr vec3 operator*(const vec3& a, double b) noexcept
{ return { a[0]*b, a[1]*b, a[2]*b }; }
constexpr vec3 operator*(double b, const vec3& a) noexcept { return a*b; }

// ==================================================================

struct vec4 {
  union {
    vec3 v3;
    double v[4];
  };
  operator const vec3&() const noexcept { return v3; }

  struct XYZT_t { } static constexpr XYZT { };
  struct PtEtaPhiM_t { } static constexpr PtEtaPhiM { };
  struct PtEtaPhiE_t { } static constexpr PtEtaPhiE { };

  constexpr vec4() noexcept: v{} { }
  constexpr vec4(double x, double y, double z, double t) noexcept
  : v{x,y,z,t} { }
  constexpr vec4(double x, double y, double z, double t, XYZT_t) noexcept
  : v{x,y,z,t} { }
  vec4(double pt, double eta, double phi, double e, PtEtaPhiE_t) noexcept
  : v3(pt,eta,phi,vec3::PtEtaPhi) {
    v[3] = e;
  }
  vec4(double pt, double eta, double phi, double m, PtEtaPhiM_t) noexcept
  : v3(pt,eta,phi,vec3::PtEtaPhi) {
    v[3] = std::sqrt(
      m >= 0
      ? sq(v[0],v[1],v[2],m)
      : std::max(sq(v[0],v[1],v[2])-sq(m),0.)
    );
  }
  template <typename U>
  vec4(const U& p) noexcept: vec4(p[0], p[1], p[2], p[3]) { }
  template <typename U>
  vec4(const U& p, XYZT_t) noexcept: vec4(p[0], p[1], p[2], p[3], XYZT) { }
  template <typename U>
  vec4(const U& p, PtEtaPhiM_t) noexcept
  : vec4(p[0], p[1], p[2], p[3], PtEtaPhiM) { }
  template <typename U>
  vec4(const U& p, PtEtaPhiE_t) noexcept
  : vec4(p[0], p[1], p[2], p[3], PtEtaPhiE) { }
  constexpr vec4(const vec3& v3, double t) noexcept: v3(v3) { v[3] = t; }

  constexpr double& operator[](unsigned i) noexcept { return v[i]; }
  constexpr const double& operator[](unsigned i) const noexcept { return v[i]; }

  constexpr double x() const noexcept { return v[0]; }
  constexpr double y() const noexcept { return v[1]; }
  constexpr double z() const noexcept { return v[2]; }
  constexpr double t() const noexcept { return v[3]; }

  constexpr double px() const noexcept { return v[0]; }
  constexpr double py() const noexcept { return v[1]; }
  constexpr double pz() const noexcept { return v[2]; }
  constexpr double e () const noexcept { return v[3]; }

  constexpr double pt2() const noexcept { return v3.pt2(); }
  double pt() const noexcept { return v3.pt(); }
  constexpr double norm2() const noexcept { return v3.norm2(); }
  double norm() const noexcept { return v3.norm(); }
  double cos_theta() const noexcept { return v3.cos_theta(); }
  double eta() const noexcept { return v3.eta(); }
  double rap() const noexcept { return 0.5*std::log((v[3]+v[2])/(v[3]-v[2])); }
  double phi() const noexcept { return v3.phi(); }
  constexpr double m2() const noexcept { return sq(v[3])-norm2(); }
  double m() const noexcept {
    const double m2_ = m2();
    return m2_ >= 0 ? std::sqrt(m2_) : -std::sqrt(-m2_);
  }
  constexpr double et2() const noexcept { return sq(v[3]) - sq(v[2]); }
  double et() const noexcept {
    const double et2_ = et2();
    return et2_ >= 0 ? std::sqrt(et2_) : -std::sqrt(-et2_);
  }

  constexpr vec3 boost_vector() const noexcept {
    return { x()/t(), y()/t(), z()/t() };
  }
  vec4& boost(const vec3& b) noexcept {
    const double b2 = b.norm2();
    const double bp = b*v3;
    const double gamma = 1. / std::sqrt(1.-b2);
    const double gamma2 = b2 > 0 ? (gamma-1.)/b2 : 0.;

    v3 += (gamma2*bp + gamma*t())*b;
    (v[3] += bp) *= gamma;

    return *this;
  }

  constexpr vec4& operator+=(const vec4& r) noexcept {
    v[0] += r[0];
    v[1] += r[1];
    v[2] += r[2];
    v[3] += r[3];
    return *this;
  }
  constexpr vec4& operator-=(const vec4& r) noexcept {
    v[0] -= r[0];
    v[1] -= r[1];
    v[2] -= r[2];
    v[3] -= r[3];
    return *this;
  }
  constexpr vec4& operator*=(double r) noexcept {
    v[0] *= r;
    v[1] *= r;
    v[2] *= r;
    v[3] *= r;
    return *this;
  }
  constexpr vec4& operator/=(double r) noexcept {
    v[0] /= r;
    v[1] /= r;
    v[2] /= r;
    v[3] /= r;
    return *this;
  }
  constexpr vec4 operator-() const noexcept {
    return { -v[0], -v[1], -v[2], -v[3] };
  }

  template <size_t I>
  friend constexpr const double& get(const vec4& v) noexcept {
    return v[I];
  }
};

constexpr vec4 operator+(const vec4& a, const vec4& b) noexcept
{ return { a[0]+b[0], a[1]+b[1], a[2]+b[2], a[3]+b[3] }; }

constexpr vec4 operator-(const vec4& a, const vec4& b) noexcept
{ return { a[0]-b[0], a[1]-b[1], a[2]-b[2], a[3]-b[3] }; }

constexpr double operator*(const vec4& a, const vec4& b) noexcept
{ return a[3]*b[3] - a.v3*b.v3; }

constexpr vec4 operator*(const vec4& a, double b) noexcept
{ return { a[0]*b, a[1]*b, a[2]*b, a[3]*b }; }
constexpr vec4 operator*(double b, const vec4& a) noexcept { return a*b; }

inline vec4 operator>>(vec4 a, const vec3& b) noexcept
{ a.boost(b); return a; }

// free functions ---------------------------------------------------

inline double cos(const vec3& a, const vec3& b) noexcept {
  const double a2b2 = a.norm2()*b.norm2();
  if (a2b2 > 0) {
    const double c = a*b/std::sqrt(a2b2);
    if (c >  1) return  1;
    if (c < -1) return -1;
    return c;
  } else {
    return 0;
  }
}

inline double dphi_mpi_pi(double dphi) noexcept {
  while (dphi >= num::pi) dphi -= num::twopi;
  while (dphi < -num::pi) dphi += num::twopi;
  return dphi;
}
inline double abs_dphi(double dphi) noexcept {
  return std::abs(dphi_mpi_pi(dphi));
}
inline double dphi(const vec3& a, const vec3& b) noexcept {
  return dphi_mpi_pi(a.phi()-b.phi());
}
inline double abs_dphi(const vec3& a, const vec3& b) noexcept {
  return std::abs(dphi(a,b));
}
inline double dphi_signed(double phi1, double phi2, double rap1, double rap2)
noexcept {
  double dphi = phi1 - phi2;
  if (rap1 < rap2) dphi = -dphi;
  return dphi_mpi_pi(dphi);
}
inline double dphi_signed(const vec4& a, const vec4& b) noexcept {
  return dphi_signed(a.phi(),b.phi(),a.rap(),b.rap());
}

inline double deltaR(double eta1, double eta2, double phi1, double phi2)
noexcept {
  return std::sqrt(sq(eta1-eta2,dphi_mpi_pi(phi1-phi2)));
}
inline double deltaR(const vec4& a, const vec4& b) noexcept {
  return deltaR(a.eta(),b.eta(),a.phi(),b.phi());
}

inline double tau(const vec4& jet, double higgs_y) noexcept {
  return std::sqrt( sq(jet[3])-sq(jet[2]) )
       / ( std::cosh(jet.rap()-higgs_y)*2 );
}
inline double pTt(const vec3& a, const vec3& b) noexcept {
  return std::abs( a[0]*b[1]-b[0]*a[1] )
       / ( std::sqrt(sq(a[0]+b[0],a[1]+b[1]))*2 );
}

} // end namespace ivanp

#endif
