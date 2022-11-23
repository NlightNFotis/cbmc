#include "options.h"

#include <util/options.h>

api_optionst api_optionst::create()
{
  return api_optionst{};
}

api_optionst &api_optionst::simplify(bool on)
{
  simplify_enabled = on;
  return *this;
}

optionst api_optionst::to_engine_options() const
{
  optionst engine_options;
  engine_options.set_option("simplify", simplify_enabled);
  return engine_options;
}
