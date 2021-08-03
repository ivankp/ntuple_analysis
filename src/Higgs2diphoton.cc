#include <chrono>
#include <cmath>

#include "Higgs2diphoton.hh"

using ivanp::vec4;

Higgs2diphoton::Higgs2diphoton(seed_type seed)
: rng(seed ? seed : std::chrono::system_clock::now().time_since_epoch().count()),
  phi_dist(0.,2*M_PI), cts_dist(-1.,1.)
{ }

Higgs2diphoton::photons_type
Higgs2diphoton::operator()(const vec_t& Higgs, bool new_kin) {
  if (new_kin) {
    const double phi = phi_dist(rng);
    const double cts = cts_dist(rng);

    const double sts = std::sin(std::acos(cts));
    const double cos_phi = std::cos(phi);
    const double sin_phi = std::sin(phi);

    cm_photon = { cos_phi*sts, sin_phi*sts, cts };
  }

  const double E = Higgs.m()/2;
  const auto boost = Higgs.boost_vector();

  auto photon = cm_photon;
  photon.rotate_u_z(boost.normalized()) *= E;

  return {
    vec4(photon, E) >> boost,
    vec4(photon,-E) >> boost
  };
}
