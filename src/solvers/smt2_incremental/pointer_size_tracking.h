// Author: Diffblue Ltd.

#ifndef CPROVER_SOLVERS_SMT2_POINTER_SIZE_TRACKING_H
#define CPROVER_SOLVERS_SMT2_POINTER_SIZE_TRACKING_H

#include <util/expr.h>

#include <unordered_map>

// Map containing pointers and their sizes
using pointer_size_mapt = 
  std::unordered_map<exprt, exprt, irep_full_hash>;

pointer_size_mapt initial_pointer_size_map();

void track_pointer_sizes(
    const exprt &expression,
    const namespacet &ns,
    pointer_size_mapt &pointer_size_map);

#endif
