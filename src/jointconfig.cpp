#include "../include/jointconfig.hpp"

#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>

namespace{

std::string trim(const std::string& s){
  const std::size_t first = s.find_first_not_of(" \t\r\n");
  if(first == std::string::npos) return "";
  const std::size_t last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

//base + name unless name is absolute
std::string resolved(const std::string& base, const std::string& name){
  return !name.empty() && name.front() == '/' ? name : base + name;
}

std::string directory(const std::string& base, const std::string& name){
  std::string result = resolved(base, name);
  if(result.back() != '/') result += '/';
  return result;
}

double positive_number(const std::map<std::string, std::string>& values, const std::string& key){
  const auto it = values.find(key);
  if(it == values.end()) throw std::invalid_argument("jointconfig: missing " + key);
  std::size_t used = 0;
  double value = std::nan("");
  try{ value = std::stod(it->second, &used); }catch(const std::exception&){}
  if(used != it->second.size() || !std::isfinite(value) || value <= 0){
    throw std::invalid_argument("jointconfig: " + key + " must be a positive number");
  }
  return value;
}

}

jointconfig::JointConfig jointconfig::load(const std::string& filename){
  std::ifstream file(filename);
  if(!file) throw std::invalid_argument("Joint config '" + filename + "' failed to open");

  static const char* const known_keys[] = {
    "NULIBConfig", "TAccConfig", "DataBoxDir", "neutron_filter",
    "PreGasDistance_mm", "PreGasEnergy_6he_MeV", "PreGasEnergy_9li_MeV"
  };
  std::map<std::string, std::string> values;
  std::string line;
  for(std::size_t line_number = 1; std::getline(file, line); ++line_number){
    line = trim(line);
    if(line.empty() || line[0] == '#') continue;
    const std::size_t eq = line.find('=');
    const std::string where = filename + " line " + std::to_string(line_number);
    if(eq == std::string::npos) throw std::invalid_argument(where + ": missing '='");
    const std::string key = trim(line.substr(0, eq));
    const std::string value = trim(line.substr(eq + 1));
    bool known = false;
    for(const char* k : known_keys) known = known || key == k;
    if(!known) throw std::invalid_argument(where + ": unknown key " + key);
    if(value.empty()) throw std::invalid_argument(where + ": empty value for " + key);
    if(!values.emplace(key, value).second) throw std::invalid_argument(where + ": repeated key " + key);
  }

  const std::size_t slash = filename.find_last_of('/');
  const std::string base = slash == std::string::npos ? "" : filename.substr(0, slash + 1);
  auto required = [&](const std::string& key){
    const auto it = values.find(key);
    if(it == values.end()) throw std::invalid_argument("jointconfig: missing " + key);
    return it->second;
  };

  JointConfig config;
  config.NULIBConfig = resolved(base, required("NULIBConfig"));
  config.TAccConfig = resolved(base, required("TAccConfig"));
  config.DataBoxDir = directory(base, values.count("DataBoxDir") ? values.at("DataBoxDir") : "../databox/");
  config.InfoDir = base.empty() ? "./" : base;
  if(values.count("neutron_filter")) config.neutron_filter = values.at("neutron_filter");
  config.PreGasDistance_mm = positive_number(values, "PreGasDistance_mm");
  config.PreGasEnergy_6he_MeV = positive_number(values, "PreGasEnergy_6he_MeV");
  config.PreGasEnergy_9li_MeV = positive_number(values, "PreGasEnergy_9li_MeV");
  return config;
}
