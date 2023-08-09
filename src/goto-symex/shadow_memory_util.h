/*******************************************************************\

Module: Symex Shadow Memory Instrumentation

Author: Peter Schrammel

\*******************************************************************/

/// \file
/// Symex Shadow Memory Instrumentation Utilities

#ifndef CPROVER_GOTO_SYMEX_SHADOW_MEMORY_UTIL_H
#define CPROVER_GOTO_SYMEX_SHADOW_MEMORY_UTIL_H

#include <util/irep.h>
#include <util/message.h>

#include "goto_symex_state.h"

#include <pointer-analysis/value_set_dereference.h>

class exprt;
class typet;

/// Extracts the field name identifier from a string expression,
/// e.g. as passed as argument to a __CPROVER_field_decl_local call.
/// \param string_expr The string argument expression
/// \return The identifier denoted by the string argument expression
irep_idt extract_field_name(const exprt &string_expr);

/// Clean the given pointer expression so that it has the right
/// shape for being used for identifying shadow memory.
/// This handles some quirks regarding array sizes containing
/// L2 symbols and string constants not having char-pointer type.
/// \param expr The pointer to the original memory, e.g. as passed to
///    __CPROVER_field_get.
/// \param type The followed type of expr.
void clean_pointer_expr(exprt &expr, const typet &type);

/// @brief Utility function for logging calls to symex_get_field.
/// @param ns (UNUSED)
/// @param log A logger in the form of a messaget. 
/// @param field_name The identifier for the field for which we perform a shadow memory get.
/// @param expr The expression for which we perform a shadow memory get 
void log_get_field(
  const namespacet &ns,
  const messaget &log,
  const irep_idt &field_name,
  const exprt &expr);

/// @brief Replaces objects marked `invalid` by a null pointer in a given expression.
/// @param expr The expression on which to perform substitution.
void replace_invalid_object_by_null(exprt &expr);

/// @brief Utility for logging a given replacement set for a pointer (the deref target value set).
/// @param ns (UNUSED)
/// @param log A logger in the form of a messaget.
/// @param value_set The value set that we want to print the values of.
void log_value_set(
  const namespacet &ns,
  const messaget &log,
  const std::vector<exprt> &value_set);

/// @brief Search for a given field's initialisation expression.
/// @param field_name The field's identifier.
/// @param state The given symex state in which we want to lookup fields.
/// @return The initialisation expression associated with the field field_name.
const exprt &
get_field_init_expr(const irep_idt &field_name, const goto_symex_statet &state);

/// @brief Given a pointer expression check if any value in its value set is compatible.
/// @param value_set The value set to lookup compatible values in.
/// @param address The pointer expression we want to check compatibility with.
/// @return True if there exist values in the value set that are compatible with
///     this expression, else false.
bool filter_by_value_set(
  const std::vector<exprt> &value_set,
  const exprt &address);

// TODO: doxygen
void log_value_set_match(
  const namespacet &ns,
  const messaget &log,
  const shadow_memory_statet::shadowed_addresst &shadowed_address,
  const exprt &matched_base_address,
  const value_set_dereferencet::valuet &dereference,
  const exprt &expr,
  const value_set_dereferencet::valuet &shadow_dereference);

/// @brief Log a match in the value set between a given pointer and expression.
/// @param ns (UNUSED)
/// @param log A logger in the form of messaget.
/// @param address The pointer expression for which we found a match.
/// @param expr The expression value we found a match for in the value set.
void log_value_set_match(
  const namespacet &ns,
  const messaget &log,
  const exprt &address,
  const exprt &expr);

// TODO: doxygen
void log_try_shadow_address(
  const namespacet &ns,
  const messaget &log,
  const shadow_memory_statet::shadowed_addresst &shadowed_address);

/// @brief Log an expression accompanied by a message.
/// @param ns 
/// @param log 
/// @param cond_text 
/// @param cond 
void log_cond(
  const namespacet &ns,
  const messaget &log,
  const char *cond_text,
  const exprt &cond);

// TODO: doxygen
exprt compute_or_over_cells(
  const exprt &expr,
  const typet &field_type,
  const namespacet &ns,
  const messaget &log,
  const bool is_union);

// TODO: doxygen
exprt compute_max_over_cells(
  const exprt &expr,
  const typet &field_type,
  const namespacet &ns,
  const messaget &log,
  const bool is_union);

#endif // CPROVER_GOTO_SYMEX_SHADOW_MEMORY_STATE_H
