#ifndef IVANP_JSON_BINNING_HH
#define IVANP_JSON_BINNING_HH

namespace nlohmann {

template <>
struct adl_serializer<std::regex> {
  static void from_json(const json& j, std::regex& x) {
    x = std::regex(
      j.template get_ref<const json::string_t&>(),
      std::regex::ECMAScript | std::regex::optimize
    );
  }
};

template <>
struct adl_serializer< ivanp::hist::uniform_axis<> > {
  using axis_t = ivanp::hist::uniform_axis<>;
  static void from_json(const json& j, axis_t& axis) {
    if (!(j.is_array() && j.size()==3)) throw std::runtime_error(
      "uniform axis definition must be of the form [min,max,ndiv]");
    axis = axis_t(j[0],j[1],j[2]);
  }
};

template <>
struct adl_serializer<
  ivanp::hist::variant_axis<
    ivanp::hist::uniform_axis<>,
    ivanp::hist::cont_axis<>
  >
> {
  using uniform_axis = ivanp::hist::uniform_axis<>;
  using cont_axis = ivanp::hist::cont_axis<>;
  using axis_t = ivanp::hist::variant_axis< uniform_axis, cont_axis >;

  static void from_json(const json& j, axis_t& axis) {
    uniform_axis u;
    cont_axis c;
    if (!(j.is_array() && j.size())) throw std::runtime_error(
      "axis definition must be a non-empty array");
    if (j.size()>1) c.edges().reserve(j.size());
    bool uniform = false;
    for (const auto& x : j) {
      if (x.is_array()) {
        u = x;
        uniform = true;
      } else {
        if (uniform) {
          c += u;
          uniform = false;
        }
        c.edges().emplace_back(x);
      }
    }
    if (uniform) {
      if (c.nedges()) {
        c += u;
      } else {
        axis = std::move(u);
        return;
      }
    }
    c.sort();
    axis = std::move(c);
  }
};

}

#endif
