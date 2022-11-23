#include <memory>
#include <vector>

// Forward declaration of API dependencies
class goto_modelt;
class message_handlert;
class optionst;

#include "options.h"

// An object in the pattern of Session Facade - owning all of the memory
// the API is using and being responsible for the management of that.
struct cbmc_api
{
  // Initialising constructor
  explicit cbmc_api();
  cbmc_api(const api_optionst &options);
  ~cbmc_api();

  /// Load a goto_model from a given vector of filenames.
  /// \param files: A vector<string> containing the filenames to be loaded
  /// \param options: An options object, to be passed on to analysis or
  /// transformation passes.
  void load_model_from_files(const std::vector<std::string> &files);

private:
  // A struct containing the API dependencies (some of them, at least)
  struct api_dependenciest
  {
    explicit api_dependenciest();
    api_dependenciest(const optionst options);
    ~api_dependenciest();
  
    std::unique_ptr<message_handlert> message_handler;
    std::unique_ptr<optionst> options;
  };
  
  std::unique_ptr<goto_modelt> model;
  std::unique_ptr<api_dependenciest> dependencies;
};
