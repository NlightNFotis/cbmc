#pragma once

#include "rust/cxx.h"

std::vector<std::string> const &translate_vector_of_string(rust::Vec<rust::String> elements);
