// Author: Diffblue Ltd.

#ifndef CPROVER_SOLVERS_SMT2_POINTER_SIZE_TRACKING_H
#define CPROVER_SOLVERS_SMT2_POINTER_SIZE_TRACKING_H

#include <util/expr.h>

#include <solvers/smt2_incremental/smt_terms.h>
#include <solvers/smt2_incremental/smt_object_size.h>
#include <solvers/smt2_incremental/object_tracking.h>

#include <unordered_map>

// Map containing pointers and their sizes

// TODO: change name (typesize_map maybe?)
using pointer_size_mapt = 
  std::unordered_map<exprt, smt_termt, irep_full_hash>;

// const smt_object_mapt &object_map,
  // const smt_object_sizet::make_applicationt &object_size)

// call conversion in here and convert to terms immediately

pointer_size_mapt initial_pointer_size_map();

void associate_pointer_sizes(
    const exprt &expression,
    const namespacet &ns,
    pointer_size_mapt &pointer_size_map,
    const smt_object_mapt &object_map,
    const smt_object_sizet::make_applicationt &object_size);

#endif
