#ifndef NLOHMANN_JSON_FASTJET_HH
#define NLOHMANN_JSON_FASTJET_HH

namespace nlohmann {

template <>
struct adl_serializer<fastjet::JetAlgorithm> {
  static void from_json(const json& j, fastjet::JetAlgorithm& alg) {
    std::string name = j;
    for (char& c : name)
      if ('A' <= c && c <= 'Z') c -= ('A'-'a');
    if (name=="kt")
      alg = fastjet::kt_algorithm;
    else if (name=="antikt" || name=="akt")
      alg = fastjet::antikt_algorithm;
    else if (name=="cambridge" || name=="ca")
      alg = fastjet::cambridge_algorithm;
    else throw std::runtime_error("Unexpected FastJet algorithm name");
  }
};

template <>
struct adl_serializer<fastjet::JetDefinition> {
  static void from_json(const json& j, fastjet::JetDefinition& def) {
    def = { j.at(0).get<fastjet::JetAlgorithm>(), j.at(1).get<double>() };
  }
};

}

#endif
