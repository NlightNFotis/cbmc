#include <vector>

#include <goto-programs/goto_model.h>

#include <util/message.h>
#include <util/options.h>
#include <util/config.h>

// A struct containing the API dependencies (some of them, at least)
struct api_depst {
    message_handlert *msg_handler;
    optionst *opts;
};

/// Initialise API dependencies
void initialize();

/// Load a goto_model from a given vector of filenames.
/// \param files: A vector<string> containing the filenames to be loaded
/// \param options: An options object, to be passed on to analysis or transformation
///   passes. 
goto_modelt load_model_from_files(
    const std::vector<std::string> &files,
    const optionst &options);
