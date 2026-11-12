#pragma once

#include <iosfwd>
#include <string>
#include <vector>

//TexNeut/TexAT run pairing for the joint analysis. The pair lists 6hepn.txt and 9lipn.txt
//live in JOINT_TNTA's info/ folder; neither detector codebase knows the other's run IDs.
namespace runpairing{
  enum class RunSet{
    None,
    SixHe,
    NineLi
  };

  enum class RunSource{
    Auto,
    TexAT,
    TexNeut
  };

  struct RunPair{
    int texneut_run{};
    int texat_run{};
  };

  struct RunLists{
    std::vector<RunPair> six_he{};
    std::vector<RunPair> nine_li{};

    bool available() const;
    //unknown IDs return None; ambiguous IDs request their detector first
    RunSet membership(int run_id, RunSource source = RunSource::Auto) const;

    //Auto detects the detector, asking only when the ID occurs in both columns.
    //Unknown IDs, conflicting mappings and unanswered prompts throw
    RunPair resolve(int run_id, RunSource source = RunSource::Auto) const;
    RunPair resolve(int run_id, std::istream& input, std::ostream& output,
                    RunSource source = RunSource::Auto) const;
  };

  //Files hold two columns, TexNeut run then TexAT run; blank lines, # comments and CRLF
  //are allowed. IDs must be positive and unique within each detector's column, across both
  //isotope sets. Failed loads leave output unchanged.
  bool load_run_list(std::vector<RunPair>& output, const std::string& filename);
  bool load_run_lists(RunLists& run_lists, const std::string& info_directory);
}
