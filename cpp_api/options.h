#pragma once

class optionst;

class api_optionst
{
  // Options for the verification engine
  bool simplify_enabled;

  // Private interface methods
  api_optionst() = default;

public:
  ~api_optionst() = default;
  static api_optionst create();

  api_optionst &simplify(bool on);

  optionst to_engine_options() const;
};
