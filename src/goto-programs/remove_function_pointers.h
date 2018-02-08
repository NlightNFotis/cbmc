/*******************************************************************\

Module: Remove Indirect Function Calls

Author: Daniel Kroening

Date: June 2003

\*******************************************************************/

/// \file
/// Remove Indirect Function Calls

#ifndef CPROVER_GOTO_PROGRAMS_REMOVE_FUNCTION_POINTERS_H
#define CPROVER_GOTO_PROGRAMS_REMOVE_FUNCTION_POINTERS_H

#include "goto_model.h"
#include "remove_const_function_pointers.h"
#include <util/message.h>

class remove_function_pointerst:public messaget
{
public:
  remove_function_pointerst(
    message_handlert &_message_handler,
    symbol_tablet &_symbol_table,
    bool _add_safety_assertion,
    bool only_resolve_const_fps,
    const goto_functionst &goto_functions);

  void operator()(goto_functionst &goto_functions);

  bool remove_function_pointers(goto_programt &goto_program);

remove_const_function_pointerst::functionst
list_potential_targets(
  goto_programt &goto_program,
  goto_programt::targett target);


protected:
  const namespacet ns;
  symbol_tablet &symbol_table;
  bool add_safety_assertion;

  // We can optionally halt the FP removal if we aren't able to use
  // remove_const_function_pointerst to sucessfully narrow to a small
  // subset of possible functions and just leave the function pointer
  // as it is.
  // This can be activated in goto-instrument using
  // --remove-const-function-pointers instead of --remove-function-pointers
  bool only_resolve_const_fps;

  void remove_function_pointer(
    goto_programt &goto_program,
    goto_programt::targett target);

  std::set<irep_idt> address_taken;

  typedef std::map<irep_idt, code_typet> type_mapt;
  type_mapt type_map;

  bool is_type_compatible(
    bool return_value_used,
    const code_typet &call_type,
    const code_typet &function_type);

  bool arg_is_type_compatible(
    const typet &call_type,
    const typet &function_type);

  void fix_argument_types(code_function_callt &function_call);
  void fix_return_type(
    code_function_callt &function_call,
    goto_programt &dest);

  void compute_address_taken_in_symbols(std::set<irep_idt> &address_taken);
};

// remove indirect function calls
// and replace by case-split
void remove_function_pointers(
  message_handlert &_message_handler,
  goto_modelt &goto_model,
  bool add_safety_assertion,
  bool only_remove_const_fps=false);

void remove_function_pointers(
  message_handlert &_message_handler,
  symbol_tablet &symbol_table,
  goto_functionst &goto_functions,
  bool add_safety_assertion,
  bool only_remove_const_fps=false);

bool remove_function_pointers(
  message_handlert &_message_handler,
  symbol_tablet &symbol_table,
  const goto_functionst &goto_functions,
  goto_programt &goto_program,
  bool add_safety_assertion,
  bool only_remove_const_fps=false);

#endif // CPROVER_GOTO_PROGRAMS_REMOVE_FUNCTION_POINTERS_H
