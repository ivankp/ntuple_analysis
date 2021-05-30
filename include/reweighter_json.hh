#ifndef NLOHMANN_JSON_REWEIGHTER_HH
#define NLOHMANN_JSON_REWEIGHTER_HH

namespace nlohmann {

template <typename T>
struct adl_serializer<std::optional<T>> {
  static void from_json(const json& j, std::optional<T>& opt) {
    if (j.is_null()) opt = std::nullopt;
    else opt = j.get<T>();
  }
  static void to_json(json& j, const std::optional<T>& opt) {
    if (opt == std::nullopt) j = nullptr;
    else j = *opt;
  }
};

template <typename T>
struct adl_serializer<reweighter::ren_fac<T>> {
  static void from_json(const json& j, reweighter::ren_fac<T>& x) {
    if (j.size()!=2) throw std::runtime_error(
      "reweighter::ren_fac must be represented as an array of size 2");
    j[0].get_to(x.ren);
    j[1].get_to(x.fac);
  }
};

template <>
struct adl_serializer<reweighter::args_struct> {
  static void from_json(const json& j, reweighter::args_struct& args) {
    args.pdf = j.at("pdf");
    args.scale = j.at("scale");
    args.pdf_var = j.at("pdf_var");
    for (const reweighter::ren_fac<double>& k : j.at("ren_fac"))
      args.add_scale(k);
  }
};

}

#endif
