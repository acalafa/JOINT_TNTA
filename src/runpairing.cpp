#include "runpairing.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace{

//detector columns must be unique, including across isotope sets; overlap between the two
//different detector columns is allowed
bool append_pair(std::vector<runpairing::RunPair>& runs, const runpairing::RunPair& pair){
  if(pair.texneut_run <= 0 || pair.texat_run <= 0) return false;
  for(const auto& existing : runs){
    if(existing.texneut_run == pair.texneut_run ||
       existing.texat_run == pair.texat_run) return false;
  }
  runs.push_back(pair);
  return true;
}

const runpairing::RunPair* find_pair(const runpairing::RunLists& lists,
                                     int run_id, runpairing::RunSource source){
  const runpairing::RunPair* found = nullptr;
  for(const auto* runs : {&lists.six_he, &lists.nine_li}){
    for(const auto& pair : *runs){
      const int candidate = source == runpairing::RunSource::TexAT
                          ? pair.texat_run : pair.texneut_run;
      if(candidate != run_id) continue;
      if(found){
        throw std::runtime_error("Conflicting run mappings for run " + std::to_string(run_id));
      }
      found = &pair;
    }
  }
  return found;
}

std::string joined_path(const std::string& directory, const std::string& filename){
  if(directory.empty()) return filename;
  return directory + (directory.back() == '/' ? "" : "/") + filename;
}

}

bool runpairing::load_run_list(std::vector<RunPair>& output, const std::string& filename){
  std::ifstream input(filename);
  if(!input.is_open()) return false;

  std::vector<RunPair> loaded;
  std::string line;
  while(std::getline(input, line)){
    //blank lines, comments, CRLF files and a final line without '\n' are fine
    const auto comment = line.find('#');
    if(comment != std::string::npos) line.erase(comment);
    std::istringstream row(line);
    row >> std::ws;
    if(row.eof()) continue;
    RunPair pair{};
    if(!(row >> pair.texneut_run >> pair.texat_run)) return false;
    row >> std::ws;
    if(!row.eof() || !append_pair(loaded, pair)) return false;
  }

  if(input.bad() || loaded.empty()) return false;
  output.swap(loaded);
  return true;
}

bool runpairing::load_run_lists(RunLists& run_lists, const std::string& info_directory){
  RunLists loaded;
  if(!load_run_list(loaded.six_he, joined_path(info_directory, "6hepn.txt")) ||
     !load_run_list(loaded.nine_li, joined_path(info_directory, "9lipn.txt"))){
    return false;
  }
  std::vector<RunPair> combined = loaded.six_he;
  for(const auto& pair : loaded.nine_li){
    if(!append_pair(combined, pair)) return false;
  }
  run_lists = std::move(loaded);
  return true;
}

bool runpairing::RunLists::available() const{
  return !six_he.empty() || !nine_li.empty();
}

runpairing::RunPair runpairing::RunLists::resolve(int run_id, RunSource source) const{
  return resolve(run_id, std::cin, std::cout, source);
}

runpairing::RunPair runpairing::RunLists::resolve(
  int run_id, std::istream& input, std::ostream& output, RunSource source
) const{
  if(source != RunSource::Auto && source != RunSource::TexAT && source != RunSource::TexNeut){
    throw std::invalid_argument("Invalid run source");
  }
  const RunPair* ta = source == RunSource::TexNeut ? nullptr
                    : find_pair(*this, run_id, RunSource::TexAT);
  const RunPair* tn = source == RunSource::TexAT ? nullptr
                    : find_pair(*this, run_id, RunSource::TexNeut);
  if(!ta && !tn){
    throw std::invalid_argument("No run pairing found for run " + std::to_string(run_id));
  }
  if(ta && tn){
    output << "Run " << run_id << " occurs in both detector lists.\n"
           << "  TexAT " << ta->texat_run << " -> TexNeut " << ta->texneut_run << '\n'
           << "  TexNeut " << tn->texneut_run << " -> TexAT " << tn->texat_run << '\n';
    for(;;){
      output << "Is the supplied run TexAT or TexNeut? [ta/tn]: " << std::flush;
      std::string answer;
      if(!(input >> answer)){
        throw std::runtime_error("Detector clarification was not provided for run " +
                                 std::to_string(run_id));
      }
      std::transform(answer.begin(), answer.end(), answer.begin(),
                     [](unsigned char ch){ return std::tolower(ch); });
      if(answer == "ta" || answer == "texat") return *ta;
      if(answer == "tn" || answer == "texneut") return *tn;
      output << "Please enter ta (TexAT) or tn (TexNeut).\n";
    }
  }
  return ta ? *ta : *tn;
}

runpairing::RunSet runpairing::RunLists::membership(int run_id, RunSource source) const{
  //None for unknown IDs, while resolving overlaps consistently
  const bool in_ta = source != RunSource::TexNeut && find_pair(*this, run_id, RunSource::TexAT);
  const bool in_tn = source != RunSource::TexAT && find_pair(*this, run_id, RunSource::TexNeut);
  if(!in_ta && !in_tn) return RunSet::None;
  const RunPair pair = resolve(run_id, source);
  for(const auto& candidate : six_he){
    if(candidate.texneut_run == pair.texneut_run &&
       candidate.texat_run == pair.texat_run) return RunSet::SixHe;
  }
  return RunSet::NineLi;
}
