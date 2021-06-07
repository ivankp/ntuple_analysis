#ifndef IVANP_HIGGS2DIPHOTON_HH
#define IVANP_HIGGS2DIPHOTON_HH

#include <array>
#include <random>

#include "ivanp/vec4.hh"

class Higgs2diphoton {
  std::mt19937 rng; // mersenne twister random number generator
  std::uniform_real_distribution<double> phi_dist; // φ
  std::uniform_real_distribution<double> cts_dist; // cos(θ*)

  ivanp::vec3 cm_photon;

public:
  using seed_type = typename decltype(rng)::result_type;
  Higgs2diphoton(seed_type seed = 0);

  using vec_t = ivanp::vec4;
  using photons_type = std::array<vec_t,2>;

  photons_type operator()(const vec_t& Higgs, bool new_kin=true);
};


#endif
