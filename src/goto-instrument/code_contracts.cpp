/*******************************************************************\

Module: Verify and use annotated invariants and pre/post-conditions

Author: Michael Tautschnig

Date: February 2016

\*******************************************************************/

/// \file
/// Verify and use annotated invariants and pre/post-conditions

#include "code_contracts.h"

#include <algorithm>
#include <map>

#include <analyses/local_may_alias.h>

#include <ansi-c/ansi_c_language.h>
#include <ansi-c/c_expr.h>
#include <ansi-c/expr2c.h>

#include <goto-programs/remove_skip.h>

#include <linking/static_lifetime_init.h>

#include <util/arith_tools.h>
#include <util/c_types.h>
#include <util/config.h>
#include <util/expr_util.h>
#include <util/fresh_symbol.h>
#include <util/mathematical_expr.h>
#include <util/mathematical_types.h>
#include <util/message.h>
#include <util/pointer_offset_size.h>
#include <util/pointer_predicates.h>
#include <util/prefix.h>
#include <util/replace_symbol.h>

bool return_value_visitort::found_return_value()
{
  return found;
}

void return_value_visitort::operator()(const exprt &exp)
{
  if(exp.id() != ID_symbol)
    return;
  const symbol_exprt &sym = to_symbol_expr(exp);
  found |= sym.get_identifier() == CPROVER_PREFIX "return_value";
}

std::set<irep_idt> &functions_in_scope_visitort::function_calls()
{
  return function_set;
}

void functions_in_scope_visitort::operator()(const goto_programt &prog)
{
  forall_goto_program_instructions(ins, prog)
  {
    if(ins->is_function_call())
    {
      const code_function_callt &call = ins->get_function_call();

      if(call.function().id() != ID_symbol)
      {
        log.error().source_location = call.find_source_location();
        log.error() << "Function pointer used in function invoked by "
                       "function contract: "
                    << messaget::eom;
        throw 0;
      }
      else
      {
        const irep_idt &fun_name =
          to_symbol_expr(call.function()).get_identifier();
        if(function_set.find(fun_name) == function_set.end())
        {
          function_set.insert(fun_name);
          auto called_fun = goto_functions.function_map.find(fun_name);
          if(called_fun == goto_functions.function_map.end())
          {
            log.warning() << "Could not find function '" << fun_name
                          << "' in goto-program." << messaget::eom;
            throw 0;
          }
          if(called_fun->second.body_available())
          {
            const goto_programt &program = called_fun->second.body;
            (*this)(program);
          }
          else
          {
            log.warning() << "No body for function: " << fun_name
                          << "invoked from function contract." << messaget::eom;
          }
        }
      }
    }
  }
}

exprt get_size(const typet &type, const namespacet &ns, messaget &log)
{
  auto size_of_opt = size_of_expr(type, ns);
  CHECK_RETURN(size_of_opt.has_value());
  exprt result = size_of_opt.value();
  result.add(ID_C_c_sizeof_type) = type;
  return result;
}

void code_contractst::check_apply_invariants(
  goto_functionst::goto_functiont &goto_function,
  const local_may_aliast &local_may_alias,
  const goto_programt::targett loop_head,
  const loopt &loop,
  const irep_idt &mode)
{
  PRECONDITION(!loop.empty());

  // find the last back edge
  goto_programt::targett loop_end = loop_head;
  for(const auto &t : loop)
    if(
      t->is_goto() && t->get_target() == loop_head &&
      t->location_number > loop_end->location_number)
      loop_end = t;

  // see whether we have an invariant
  exprt invariant = static_cast<const exprt &>(
    loop_end->get_condition().find(ID_C_spec_loop_invariant));
  if(invariant.is_nil())
    return;

  // change
  //   H: loop;
  //   E: ...
  // to
  //   H: assert(invariant);
  //   havoc;
  //   assume(invariant);
  //   if(guard) goto E:
  //   loop;
  //   assert(invariant);
  //   assume(false);
  //   E: ...

  // find out what can get changed in the loop
  modifiest modifies;
  get_modifies(local_may_alias, loop, modifies);

  // build the havocking code
  goto_programt havoc_code;

  // process quantified variables correctly (introduce a fresh temporary)
  // and return a copy of the invariant
  const auto &invariant_expr = [&]() {
    auto invariant_copy = invariant;
    replace_symbolt replace;
    code_contractst::add_quantified_variable(invariant_copy, replace, mode);
    replace(invariant_copy);
    return invariant_copy;
  };

  // Generate: assert(invariant) just before the loop
  // We use a block scope to create a temporary assertion,
  // and immediately convert it to goto instructions.
  {
    code_assertt assertion{invariant_expr()};
    assertion.add_source_location() = loop_head->source_location;
    converter.goto_convert(assertion, havoc_code, mode);
    havoc_code.instructions.back().source_location.set_comment(
      "Check loop invariant before entry");
  }

  // havoc variables being written to
  build_havoc_code(loop_head, modifies, havoc_code);

  // Generate: assume(invariant) just after havocing
  // We use a block scope to create a temporary assumption,
  // and immediately convert it to goto instructions.
  {
    code_assumet assumption{invariant_expr()};
    assumption.add_source_location() = loop_head->source_location;
    converter.goto_convert(assumption, havoc_code, mode);
  }

  // non-deterministically skip the loop if it is a do-while loop
  if(!loop_head->is_goto())
  {
    havoc_code.add(goto_programt::make_goto(
      loop_end,
      side_effect_expr_nondett(bool_typet(), loop_head->source_location)));
  }

  // Now havoc at the loop head.
  // Use insert_before_swap to preserve jumps to loop head.
  goto_function.body.insert_before_swap(loop_head, havoc_code);

  // Generate: assert(invariant) just after the loop exits
  // We use a block scope to create a temporary assertion,
  // and immediately convert it to goto instructions.
  {
    code_assertt assertion{invariant_expr()};
    assertion.add_source_location() = loop_end->source_location;
    converter.goto_convert(assertion, havoc_code, mode);
    havoc_code.instructions.back().source_location.set_comment(
      "Check that loop invariant is preserved");
    auto offset = havoc_code.instructions.size();
    goto_function.body.insert_before_swap(loop_end, havoc_code);
    std::advance(loop_end, offset);
  }

  // change the back edge into assume(false) or assume(guard)
  loop_end->targets.clear();
  loop_end->type=ASSUME;
  if(loop_head->is_goto())
    loop_end->set_condition(false_exprt());
  else
    loop_end->set_condition(boolean_negate(loop_end->get_condition()));
}

bool code_contractst::has_contract(const irep_idt fun_name)
{
  const symbolt &function_symbol = ns.lookup(fun_name);
  const auto &type = to_code_with_contract_type(function_symbol.type);
  return type.has_contract();
}

void code_contractst::add_quantified_variable(
  const exprt &expression,
  replace_symbolt &replace,
  const irep_idt &mode)
{
  if(expression.id() == ID_not || expression.id() == ID_typecast)
  {
    // For unary connectives, recursively check for
    // nested quantified formulae in the term
    const auto &unary_expression = to_unary_expr(expression);
    add_quantified_variable(unary_expression.op(), replace, mode);
  }
  if(expression.id() == ID_notequal || expression.id() == ID_implies)
  {
    // For binary connectives, recursively check for
    // nested quantified formulae in the left and right terms
    const auto &binary_expression = to_binary_expr(expression);
    add_quantified_variable(binary_expression.lhs(), replace, mode);
    add_quantified_variable(binary_expression.rhs(), replace, mode);
  }
  if(expression.id() == ID_if)
  {
    // For ternary connectives, recursively check for
    // nested quantified formulae in all three terms
    const auto &if_expression = to_if_expr(expression);
    add_quantified_variable(if_expression.cond(), replace, mode);
    add_quantified_variable(if_expression.true_case(), replace, mode);
    add_quantified_variable(if_expression.false_case(), replace, mode);
  }
  if(expression.id() == ID_and || expression.id() == ID_or)
  {
    // For multi-ary connectives, recursively check for
    // nested quantified formulae in all terms
    const auto &multi_ary_expression = to_multi_ary_expr(expression);
    for(const auto &operand : multi_ary_expression.operands())
    {
      add_quantified_variable(operand, replace, mode);
    }
  }
  else if(expression.id() == ID_exists || expression.id() == ID_forall)
  {
    // When a quantifier expression is found,
    // for each quantified variable ...
    const auto &quantifier_expression = to_quantifier_expr(expression);
    for(const auto &quantified_variable : quantifier_expression.variables())
    {
      const auto &quantified_symbol = to_symbol_expr(quantified_variable);

      // 1. create fresh symbol
      symbolt new_symbol = get_fresh_aux_symbol(
        quantified_symbol.type(),
        id2string(quantified_symbol.get_identifier()),
        "tmp",
        quantified_symbol.source_location(),
        mode,
        symbol_table);

      // 2. add created fresh symbol to expression map
      symbol_exprt q(
        quantified_symbol.get_identifier(), quantified_symbol.type());
      replace.insert(q, new_symbol.symbol_expr());

      // 3. recursively check for nested quantified formulae
      add_quantified_variable(quantifier_expression.where(), replace, mode);
    }
  }
}

void code_contractst::replace_old_parameter(
  exprt &expr,
  std::map<exprt, exprt> &parameter2history,
  source_locationt location,
  const irep_idt &function,
  const irep_idt &mode,
  goto_programt &history)
{
  for(auto &op : expr.operands())
  {
    replace_old_parameter(
      op, parameter2history, location, function, mode, history);
  }

  if(expr.id() == ID_old)
  {
    DATA_INVARIANT(
      expr.operands().size() == 1, CPROVER_PREFIX "old must have one operand");

    const auto &parameter = to_old_expr(expr).expression();

    // TODO: generalize below
    if(parameter.id() == ID_dereference)
    {
      const auto &dereference_expr = to_dereference_expr(parameter);

      auto it = parameter2history.find(dereference_expr);

      if(it == parameter2history.end())
      {
        // 1. Create a temporary symbol expression that represents the
        // history variable
        symbol_exprt tmp_symbol =
          new_tmp_symbol(dereference_expr.type(), location, function, mode)
            .symbol_expr();

        // 2. Associate the above temporary variable to it's corresponding
        // expression
        parameter2history[dereference_expr] = tmp_symbol;

        // 3. Add the required instructions to the instructions list
        // 3.1 Declare the newly created temporary variable
        history.add(goto_programt::make_decl(tmp_symbol, location));

        // 3.2 Add an assignment such that the value pointed to by the new
        // temporary variable is equal to the value of the corresponding
        // parameter
        history.add(goto_programt::make_assignment(
          tmp_symbol, dereference_expr, location));
      }

      expr = parameter2history[dereference_expr];
    }
    else
    {
      log.error() << CPROVER_PREFIX "old does not currently support "
                  << parameter.id() << " expressions." << messaget::eom;
      throw 0;
    }
  }
}

std::pair<goto_programt, goto_programt>
code_contractst::create_ensures_instruction(
  codet &expression,
  source_locationt location,
  const irep_idt &function,
  const irep_idt &mode)
{
  std::map<exprt, exprt> parameter2history;
  goto_programt history;

  // Find and replace "old" expression in the "expression" variable
  replace_old_parameter(
    expression, parameter2history, location, function, mode, history);

  // Create instructions corresponding to the ensures clause
  goto_programt ensures_program;
  converter.goto_convert(expression, ensures_program, mode);

  // return a pair containing:
  // 1. instructions corresponding to the ensures clause
  // 2. instructions related to initializing the history variables
  return std::make_pair(std::move(ensures_program), std::move(history));
}

bool code_contractst::apply_function_contract(
  const irep_idt &function_id,
  goto_programt &goto_program,
  goto_programt::targett target)
{
  const code_function_callt &call = target->get_function_call();

  // Return if the function is not named in the call; currently we don't handle
  // function pointers.
  PRECONDITION(call.function().id() == ID_symbol);

  // Retrieve the function type, which is needed to extract the contract
  // components.
  const irep_idt &function = to_symbol_expr(call.function()).get_identifier();
  const symbolt &function_symbol = ns.lookup(function);
  const auto &type = to_code_with_contract_type(function_symbol.type);

  // Isolate each component of the contract.
  auto assigns = type.assigns();
  auto requires = conjunction(type.requires());
  auto ensures = conjunction(type.ensures());

  // Check to see if the function contract actually constrains its effect on
  // the program state; if not, return.
  if(ensures.is_true() && assigns.is_nil())
    return false;

  // Create a replace_symbolt object, for replacing expressions in the callee
  // with expressions from the call site (e.g. the return value).
  // This object tracks replacements that are common to ENSURES and REQUIRES.
  replace_symbolt common_replace;
  if(type.return_type() != empty_typet())
  {
    // Check whether the function's return value is not disregarded.
    if(call.lhs().is_not_nil())
    {
      // If so, have it replaced appropriately.
      // For example, if foo() ensures that its return value is > 5, then
      // rewrite calls to foo as follows:
      // x = foo() -> assume(__CPROVER_return_value > 5) -> assume(x > 5)
      symbol_exprt ret_val(CPROVER_PREFIX "return_value", call.lhs().type());
      common_replace.insert(ret_val, call.lhs());
    }
    else
    {
      // If the function does return a value, but the return value is
      // disregarded, check if the postcondition includes the return value.
      return_value_visitort v;
      ensures.visit(v);
      if(v.found_return_value())
      {
        // The postcondition does mention __CPROVER_return_value, so mint a
        // fresh variable to replace __CPROVER_return_value with.
        const symbolt &fresh = get_fresh_aux_symbol(
          type.return_type(),
          id2string(function),
          "ignored_return_value",
          call.source_location(),
          symbol_table.lookup_ref(function).mode,
          ns,
          symbol_table);
        symbol_exprt ret_val(CPROVER_PREFIX "return_value", type.return_type());
        common_replace.insert(ret_val, fresh.symbol_expr());
      }
    }
  }

  // Replace formal parameters
  auto a_it = call.arguments().begin();
  for(auto p_it = type.parameters().begin();
      p_it != type.parameters().end() && a_it != call.arguments().end();
      ++p_it, ++a_it)
  {
    if(!p_it->get_identifier().empty())
    {
      symbol_exprt p(p_it->get_identifier(), p_it->type());
      common_replace.insert(p, *a_it);
    }
  }

  // ASSIGNS clause should not refer to any quantified variables,
  // and only refer to the common symbols to be replaced.
  common_replace(assigns);

  const auto &mode = symbol_table.lookup_ref(function).mode;

  is_fresh_replacet is_fresh(*this, log, function);
  is_fresh.create_declarations();

  // Insert assertion of the precondition immediately before the call site.
  if(requires.is_not_nil())
  {
    replace_symbolt replace(common_replace);
    code_contractst::add_quantified_variable(requires, replace, mode);
    replace(requires);

    goto_programt assertion;
    converter.goto_convert(
      code_assertt(requires),
      assertion,
      symbol_table.lookup_ref(function).mode);
    assertion.instructions.back().source_location = requires.source_location();
    assertion.instructions.back().source_location.set_comment(
      "Check requires clause");
    assertion.instructions.back().source_location.set_property_class(
      ID_precondition);
    is_fresh.update_requires(assertion);
    auto lines_to_iterate = assertion.instructions.size();
    goto_program.insert_before_swap(target, assertion);
    std::advance(target, lines_to_iterate);
  }

  // Gather all the instructions required to handle history variables
  // as well as the ensures clause
  std::pair<goto_programt, goto_programt> ensures_pair;
  if(ensures.is_not_nil())
  {
    replace_symbolt replace(common_replace);
    code_contractst::add_quantified_variable(ensures, replace, mode);
    replace(ensures);

    auto assumption = code_assumet(ensures);
    ensures_pair = create_ensures_instruction(
      assumption,
      ensures.source_location(),
      function,
      symbol_table.lookup_ref(function).mode);

    // add all the history variable initialization instructions
    // to the goto program
    auto lines_to_iterate = ensures_pair.second.instructions.size();
    goto_program.insert_before_swap(target, ensures_pair.second);
    std::advance(target, lines_to_iterate);
  }

  // Create a series of non-deterministic assignments to havoc the variables
  // in the assigns clause.
  if(assigns.is_not_nil())
  {
    assigns_clauset assigns_cause(assigns, *this, function_id, log);
    goto_programt assigns_havoc = assigns_cause.havoc_code(
      function_symbol.location, function_id, function_symbol.mode);

    // Insert the non-deterministic assignment immediately before the call site.
    std::size_t lines_to_iterate = assigns_havoc.instructions.size();
    goto_program.insert_before_swap(target, assigns_havoc);
    std::advance(target, lines_to_iterate);
  }

  // To remove the function call, insert statements related to the assumption.
  // Then, replace the function call with a SKIP statement.
  if(ensures.is_not_nil())
  {
    is_fresh.update_ensures(ensures_pair.first);
    auto lines_to_iterate = ensures_pair.first.instructions.size();
    goto_program.insert_before_swap(target, ensures_pair.first);
    std::advance(target, lines_to_iterate);
  }
  *target = goto_programt::make_skip();

  // Add this function to the set of replaced functions.
  summarized.insert(function);
  return false;
}

void code_contractst::apply_loop_contract(
  const irep_idt &function_name,
  goto_functionst::goto_functiont &goto_function)
{
  local_may_aliast local_may_alias(goto_function);
  natural_loops_mutablet natural_loops(goto_function.body);

  // Iterate over the (natural) loops in the function,
  // and apply any invariant annotations that we find.
  for(const auto &loop : natural_loops.loop_map)
    check_apply_invariants(
      goto_function,
      local_may_alias,
      loop.first,
      loop.second,
      symbol_table.lookup_ref(function_name).mode);
}

const symbolt &code_contractst::new_tmp_symbol(
  const typet &type,
  const source_locationt &source_location,
  const irep_idt &function_id,
  const irep_idt &mode)
{
  return get_fresh_aux_symbol(
    type,
    id2string(function_id) + "::tmp_cc",
    "tmp_cc",
    source_location,
    mode,
    symbol_table);
}

const namespacet &code_contractst::get_namespace() const
{
  return ns;
}

symbol_tablet &code_contractst::get_symbol_table()
{
  return symbol_table;
}

goto_functionst &code_contractst::get_goto_functions()
{
  return goto_functions;
}

exprt code_contractst::create_alias_expression(
  const exprt &lhs,
  std::vector<exprt> &aliasable_references)
{
  exprt::operandst operands;
  operands.reserve(aliasable_references.size());
  for(auto aliasable : aliasable_references)
  {
    operands.push_back(equal_exprt(lhs, typecast_exprt(aliasable, lhs.type())));
  }
  return disjunction(operands);
}

void code_contractst::instrument_assign_statement(
  goto_programt::instructionst::iterator &instruction_iterator,
  goto_programt &program,
  exprt &assigns,
  std::set<irep_idt> &freely_assignable_symbols,
  assigns_clauset &assigns_clause)
{
  INVARIANT(
    instruction_iterator->is_assign(),
    "The first instruction of instrument_assign_statement should always be"
    " an assignment");

  const exprt &lhs = instruction_iterator->assign_lhs();

  goto_programt alias_assertion;
  alias_assertion.add(goto_programt::make_assertion(
    assigns_clause.alias_expression(lhs),
    instruction_iterator->source_location));
  alias_assertion.instructions.back().source_location.set_comment(
    "Check that " + from_expr(ns, lhs.id(), lhs) + " is assignable");
  int lines_to_iterate = alias_assertion.instructions.size();
  program.insert_before_swap(instruction_iterator, alias_assertion);
  std::advance(instruction_iterator, lines_to_iterate);
}

void code_contractst::instrument_call_statement(
  goto_programt::instructionst::iterator &instruction_iterator,
  goto_programt &program,
  exprt &assigns,
  const irep_idt &function_id,
  std::set<irep_idt> &freely_assignable_symbols,
  assigns_clauset &assigns_clause)
{
  INVARIANT(
    instruction_iterator->is_function_call(),
    "The first argument of instrument_call_statement should always be "
    "a function call");

  code_function_callt call = instruction_iterator->get_function_call();
  irep_idt called_name;
  if(call.function().id() == ID_dereference)
  {
    called_name = to_symbol_expr(to_dereference_expr(call.function()).pointer())
                    .get_identifier();
  }
  else
  {
    called_name = to_symbol_expr(call.function()).get_identifier();
  }

  if(called_name == "malloc")
  {
    goto_programt::instructionst::iterator local_instruction_iterator =
      instruction_iterator;
    // malloc statments return a void pointer, which is then cast and assigned
    // to a result variable. We iterate one line forward to grab the result of
    // the malloc once it is cast.
    local_instruction_iterator++;
    if(local_instruction_iterator->is_assign())
    {
      const exprt &rhs = local_instruction_iterator->assign_rhs();
      INVARIANT(
        rhs.id() == ID_typecast,
        "malloc is called but the result is not cast. Excluding result from "
        "the assignable memory frame.");
      typet cast_type = rhs.type();

      // Make freshly allocated memory assignable, if we can determine its type.
      assigns_clause_targett *new_target =
        assigns_clause.add_pointer_target(rhs);
      goto_programt &pointer_capture = new_target->get_init_block();

      int lines_to_iterate = pointer_capture.instructions.size();
      program.insert_before_swap(local_instruction_iterator, pointer_capture);
      std::advance(instruction_iterator, lines_to_iterate + 1);
    }
    return; // assume malloc edits no pre-existing memory objects.
  }

  if(
    call.lhs().is_not_nil() && call.lhs().id() == ID_symbol &&
    freely_assignable_symbols.find(
      to_symbol_expr(call.lhs()).get_identifier()) ==
      freely_assignable_symbols.end())
  {
    exprt alias_expr = assigns_clause.alias_expression(call.lhs());

    goto_programt alias_assertion;
    alias_assertion.add(goto_programt::make_assertion(
      alias_expr, instruction_iterator->source_location));
    alias_assertion.instructions.back().source_location.set_comment(
      "Check that " + from_expr(ns, alias_expr.id(), alias_expr) +
      " is assignable");
    program.insert_before_swap(instruction_iterator, alias_assertion);
    ++instruction_iterator;
  }

  const symbolt &called_symbol = ns.lookup(called_name);
  // Called symbol might be a function pointer.
  const typet &called_symbol_type = (called_symbol.type.id() == ID_pointer)
                                      ? called_symbol.type.subtype()
                                      : called_symbol.type;
  exprt called_assigns =
    to_code_with_contract_type(called_symbol_type).assigns();
  const code_typet &called_type = to_code_type(called_symbol_type);

  if(called_assigns.is_not_nil())
  {
    replace_symbolt replace_formal_params;
    auto a_it = call.arguments().begin();
    for(auto p_it = called_type.parameters().begin();
        p_it != called_type.parameters().end() &&
        a_it != call.arguments().end();
        ++p_it, ++a_it)
    {
      if(!p_it->get_identifier().empty())
      {
        symbol_exprt p(p_it->get_identifier(), p_it->type());
        replace_formal_params.insert(p, *a_it);
      }
    }
    replace_formal_params(called_assigns);

    // check compatibility of assigns clause with the called function
    assigns_clauset called_assigns_clause(
      called_assigns, *this, function_id, log);
    exprt compatible =
      assigns_clause.compatible_expression(called_assigns_clause);
    goto_programt alias_assertion;
    alias_assertion.add(goto_programt::make_assertion(
      compatible, instruction_iterator->source_location));
    alias_assertion.instructions.back().source_location.set_comment(
      "Check compatibility of assigns clause with the called function");
    program.insert_before_swap(instruction_iterator, alias_assertion);
    ++instruction_iterator;
  }
}

bool code_contractst::check_for_looped_mallocs(const goto_programt &program)
{
  // Collect all GOTOs and mallocs
  std::vector<goto_programt::instructiont> back_gotos;
  std::vector<goto_programt::instructiont> malloc_calls;

  int index = 0;
  for(goto_programt::instructiont instruction : program.instructions)
  {
    if(instruction.is_backwards_goto())
    {
      back_gotos.push_back(instruction);
    }
    if(instruction.is_function_call())
    {
      code_function_callt call = instruction.get_function_call();
      irep_idt called_name;
      if(call.function().id() == ID_dereference)
      {
        called_name =
          to_symbol_expr(to_dereference_expr(call.function()).pointer())
            .get_identifier();
      }
      else
      {
        called_name = to_symbol_expr(call.function()).get_identifier();
      }

      if(called_name == "malloc")
      {
        malloc_calls.push_back(instruction);
      }
    }
    index++;
  }
  // Make sure there are no gotos that go back such that a malloc
  // is between the goto and its destination (possible loop).
  for(auto goto_entry : back_gotos)
  {
    for(const auto &target : goto_entry.targets)
    {
      for(auto malloc_entry : malloc_calls)
      {
        if(
          malloc_entry.location_number >= target->location_number &&
          malloc_entry.location_number < goto_entry.location_number)
        {
          log.error() << "Call to malloc at location "
                      << malloc_entry.location_number << " falls between goto "
                      << "source location " << goto_entry.location_number
                      << " and it's destination at location "
                      << target->location_number << ". "
                      << "Unable to complete instrumentation, as this malloc "
                      << "may be in a loop." << messaget::eom;
          throw 0;
        }
      }
    }
  }
  return false;
}

bool code_contractst::add_pointer_checks(const std::string &function_name)
{
  // Get the function object before instrumentation.
  auto old_function = goto_functions.function_map.find(function_name);
  if(old_function == goto_functions.function_map.end())
  {
    log.error() << "Could not find function '" << function_name
                << "' in goto-program; not enforcing contracts."
                << messaget::eom;
    return true;
  }
  goto_programt &program = old_function->second.body;
  if(program.instructions.empty()) // empty function body
  {
    return false;
  }

  const irep_idt function_id(function_name);
  const symbolt &function_symbol = ns.lookup(function_id);
  const auto &type = to_code_with_contract_type(function_symbol.type);

  exprt assigns_expr = type.assigns();

  assigns_clauset assigns(assigns_expr, *this, function_id, log);

  goto_programt::instructionst::iterator instruction_it =
    program.instructions.begin();

  // Create temporary variables to hold the assigns
  // clause targets before they can be modified.
  goto_programt standin_decls = assigns.init_block(function_symbol.location);
  goto_programt mark_dead = assigns.dead_stmts(
    function_symbol.location, function_name, function_symbol.mode);

  // Create a list of variables that are okay to assign.
  std::set<irep_idt> freely_assignable_symbols;
  for(code_typet::parametert param : type.parameters())
  {
    freely_assignable_symbols.insert(param.get_identifier());
  }

  int lines_to_iterate = standin_decls.instructions.size();
  program.insert_before_swap(instruction_it, standin_decls);
  std::advance(instruction_it, lines_to_iterate);

  if(check_for_looped_mallocs(program))
  {
    return true;
  }

  // Insert aliasing assertions
  for(; instruction_it != program.instructions.end(); ++instruction_it)
  {
    if(instruction_it->is_decl())
    {
      freely_assignable_symbols.insert(
        instruction_it->get_decl().symbol().get_identifier());

      assigns_clause_targett *new_target =
        assigns.add_target(instruction_it->get_decl().symbol());
      goto_programt &pointer_capture = new_target->get_init_block();

      lines_to_iterate = pointer_capture.instructions.size();
      for(auto in : pointer_capture.instructions)
      {
        program.insert_after(instruction_it, in);
        ++instruction_it;
      }
    }
    else if(instruction_it->is_assign())
    {
      instrument_assign_statement(
        instruction_it,
        program,
        assigns_expr,
        freely_assignable_symbols,
        assigns);
    }
    else if(instruction_it->is_function_call())
    {
      instrument_call_statement(
        instruction_it,
        program,
        assigns_expr,
        function_id,
        freely_assignable_symbols,
        assigns);
    }
  }

  // Walk the iterator back to the last statement
  while(!instruction_it->is_end_function())
  {
    --instruction_it;
  }

  // Make sure the temporary symbols are marked dead
  lines_to_iterate = mark_dead.instructions.size();
  program.insert_before_swap(instruction_it, mark_dead);

  return false;
}

bool code_contractst::enforce_contract(const std::string &fun_to_enforce)
{
  // Add statements to the source function
  // to ensure assigns clause is respected.
  add_pointer_checks(fun_to_enforce);

  // Rename source function
  std::stringstream ss;
  ss << CPROVER_PREFIX << "contracts_original_" << fun_to_enforce;
  const irep_idt mangled(ss.str());
  const irep_idt original(fun_to_enforce);

  auto old_function = goto_functions.function_map.find(original);
  if(old_function == goto_functions.function_map.end())
  {
    log.error() << "Could not find function '" << fun_to_enforce
                << "' in goto-program; not enforcing contracts."
                << messaget::eom;
    return true;
  }

  std::swap(goto_functions.function_map[mangled], old_function->second);
  goto_functions.function_map.erase(old_function);

  // Place a new symbol with the mangled name into the symbol table
  source_locationt sl;
  sl.set_file("instrumented for code contracts");
  sl.set_line("0");
  symbolt mangled_sym;
  const symbolt *original_sym = symbol_table.lookup(original);
  mangled_sym = *original_sym;
  mangled_sym.name = mangled;
  mangled_sym.base_name = mangled;
  mangled_sym.location = sl;
  const auto mangled_found = symbol_table.insert(std::move(mangled_sym));
  INVARIANT(
    mangled_found.second,
    "There should be no existing function called " + ss.str() +
      " in the symbol table because that name is a mangled name");

  // Insert wrapper function into goto_functions
  auto nexist_old_function = goto_functions.function_map.find(original);
  INVARIANT(
    nexist_old_function == goto_functions.function_map.end(),
    "There should be no function called " + fun_to_enforce +
      " in the function map because that function should have had its"
      " name mangled");

  auto mangled_fun = goto_functions.function_map.find(mangled);
  INVARIANT(
    mangled_fun != goto_functions.function_map.end(),
    "There should be a function called " + ss.str() +
      " in the function map because we inserted a fresh goto-program"
      " with that mangled name");

  goto_functiont &wrapper = goto_functions.function_map[original];
  wrapper.parameter_identifiers = mangled_fun->second.parameter_identifiers;
  wrapper.body.add(goto_programt::make_end_function(sl));
  add_contract_check(original, mangled, wrapper.body);

  return false;
}

void code_contractst::add_contract_check(
  const irep_idt &wrapper_fun,
  const irep_idt &mangled_fun,
  goto_programt &dest)
{
  PRECONDITION(!dest.instructions.empty());

  const symbolt &function_symbol = ns.lookup(mangled_fun);
  const auto &code_type = to_code_with_contract_type(function_symbol.type);

  exprt assigns = code_type.assigns();
  exprt requires = conjunction(code_type.requires());
  exprt ensures = conjunction(code_type.ensures());

  INVARIANT(
    !ensures.is_true() || assigns.is_not_nil(),
    "Code contract enforcement is trivial without an ensures or assigns "
    "clause.");

  // build:
  // if(nondet)
  //   decl ret
  //   decl parameter1 ...
  //   decl history_parameter1 ... [optional]
  //   assume(requires)  [optional]
  //   ret=function(parameter1, ...)
  //   assert(ensures)
  // skip: ...

  // build skip so that if(nondet) can refer to it
  goto_programt tmp_skip;
  goto_programt::targett skip =
    tmp_skip.add(goto_programt::make_skip(ensures.source_location()));

  goto_programt check;

  // prepare function call including all declarations
  code_function_callt call(function_symbol.symbol_expr());

  // Create a replace_symbolt object, for replacing expressions in the callee
  // with expressions from the call site (e.g. the return value).
  // This object tracks replacements that are common to ENSURES and REQUIRES.
  replace_symbolt common_replace;

  // decl ret
  code_returnt return_stmt;
  if(code_type.return_type() != empty_typet())
  {
    symbol_exprt r = new_tmp_symbol(
                       code_type.return_type(),
                       skip->source_location,
                       wrapper_fun,
                       function_symbol.mode)
                       .symbol_expr();
    check.add(goto_programt::make_decl(r, skip->source_location));

    call.lhs() = r;
    return_stmt = code_returnt(r);

    symbol_exprt ret_val(CPROVER_PREFIX "return_value", call.lhs().type());
    common_replace.insert(ret_val, r);
  }

  // decl parameter1 ...
  goto_functionst::function_mapt::iterator f_it =
    goto_functions.function_map.find(mangled_fun);
  PRECONDITION(f_it != goto_functions.function_map.end());

  const goto_functionst::goto_functiont &gf = f_it->second;
  for(const auto &parameter : gf.parameter_identifiers)
  {
    PRECONDITION(!parameter.empty());
    const symbolt &parameter_symbol = ns.lookup(parameter);
    symbol_exprt p = new_tmp_symbol(
                       parameter_symbol.type,
                       skip->source_location,
                       wrapper_fun,
                       parameter_symbol.mode)
                       .symbol_expr();
    check.add(goto_programt::make_decl(p, skip->source_location));
    check.add(goto_programt::make_assignment(
      p, parameter_symbol.symbol_expr(), skip->source_location));

    call.arguments().push_back(p);

    common_replace.insert(parameter_symbol.symbol_expr(), p);
  }

  is_fresh_enforcet visitor(*this, log, wrapper_fun);
  visitor.create_declarations();

  // Generate: assume(requires)
  if(requires.is_not_nil())
  {
    // extend common_replace with quantified variables in REQUIRES,
    // and then do the replacement
    replace_symbolt replace(common_replace);
    code_contractst::add_quantified_variable(
      requires, replace, function_symbol.mode);
    replace(requires);

    goto_programt assumption;
    converter.goto_convert(
      code_assumet(requires), assumption, function_symbol.mode);
    visitor.update_requires(assumption);
    check.destructive_append(assumption);
  }

  // Prepare the history variables handling
  std::pair<goto_programt, goto_programt> ensures_pair;

  // Generate: copies for history variables
  if(ensures.is_not_nil())
  {
    // extend common_replace with quantified variables in ENSURES,
    // and then do the replacement
    replace_symbolt replace(common_replace);
    code_contractst::add_quantified_variable(
      ensures, replace, function_symbol.mode);
    replace(ensures);

    // get all the relevant instructions related to history variables
    auto assertion = code_assertt(ensures);
    assertion.add_source_location() = ensures.source_location();
    ensures_pair = create_ensures_instruction(
      assertion, ensures.source_location(), wrapper_fun, function_symbol.mode);
    ensures_pair.first.instructions.back().source_location.set_comment(
      "Check ensures clause");
    ensures_pair.first.instructions.back().source_location.set_property_class(
      ID_postcondition);

    // add all the history variable initializations
    visitor.update_ensures(ensures_pair.first);
    check.destructive_append(ensures_pair.second);
  }

  // ret=mangled_fun(parameter1, ...)
  check.add(goto_programt::make_function_call(call, skip->source_location));

  // Generate: assert(ensures)
  if(ensures.is_not_nil())
  {
    check.destructive_append(ensures_pair.first);
  }

  if(code_type.return_type() != empty_typet())
  {
    check.add(goto_programt::make_return(return_stmt, skip->source_location));
  }

  // prepend the new code to dest
  check.destructive_append(tmp_skip);
  dest.destructive_insert(dest.instructions.begin(), check);
}

bool code_contractst::replace_calls(
  const std::set<std::string> &funs_to_replace)
{
  bool fail = false;
  for(const auto &fun : funs_to_replace)
  {
    if(!has_contract(fun))
    {
      log.error() << "Function '" << fun
                  << "' does not have a contract; "
                     "not replacing calls with contract."
                  << messaget::eom;
      fail = true;
    }
  }
  if(fail)
    return true;

  for(auto &goto_function : goto_functions.function_map)
  {
    Forall_goto_program_instructions(ins, goto_function.second.body)
    {
      if(ins->is_function_call())
      {
        const code_function_callt &call = ins->get_function_call();

        if(call.function().id() != ID_symbol)
          continue;

        const irep_idt &function_name =
          to_symbol_expr(call.function()).get_identifier();
        auto found = std::find(
          funs_to_replace.begin(),
          funs_to_replace.end(),
          id2string(function_name));
        if(found == funs_to_replace.end())
          continue;

        fail |= apply_function_contract(
          function_name, goto_function.second.body, ins);
      }
    }
  }

  if(fail)
    return true;

  for(auto &goto_function : goto_functions.function_map)
    remove_skip(goto_function.second.body);

  goto_functions.update();

  return false;
}

bool code_contractst::replace_calls()
{
  std::set<std::string> funs_to_replace;
  for(auto &goto_function : goto_functions.function_map)
  {
    if(has_contract(goto_function.first))
      funs_to_replace.insert(id2string(goto_function.first));
  }
  return replace_calls(funs_to_replace);
}

bool code_contractst::enforce_contracts()
{
  std::set<std::string> funs_to_enforce;
  for(auto &goto_function : goto_functions.function_map)
  {
    if(has_contract(goto_function.first))
      funs_to_enforce.insert(id2string(goto_function.first));
    else
      apply_loop_contract(goto_function.first, goto_function.second);
  }
  return enforce_contracts(funs_to_enforce);
}

bool code_contractst::enforce_contracts(
  const std::set<std::string> &funs_to_enforce)
{
  bool fail = false;
  for(const auto &fun : funs_to_enforce)
  {
    auto goto_function = goto_functions.function_map.find(fun);
    if(goto_function == goto_functions.function_map.end())
    {
      fail = true;
      log.error() << "Could not find function '" << fun
                  << "' in goto-program; not enforcing contracts."
                  << messaget::eom;
      continue;
    }
    apply_loop_contract(goto_function->first, goto_function->second);

    if(!has_contract(fun))
    {
      fail = true;
      log.error() << "Could not find any contracts within function '" << fun
                  << "'; nothing to enforce." << messaget::eom;
      continue;
    }

    if(!fail)
      fail = enforce_contract(fun);
  }
  return fail;
}

assigns_clause_scalar_targett::assigns_clause_scalar_targett(
  const exprt &object_ptr,
  code_contractst &contract,
  messaget &log_parameter,
  const irep_idt &function_id)
  : assigns_clause_targett(
      Scalar,
      pointer_for(object_ptr),
      contract,
      log_parameter),
    local_standin_variable(typet())
{
  const symbolt &function_symbol = contract.ns.lookup(function_id);

  // Declare a new symbol to stand in for the reference
  symbolt standin_symbol = contract.new_tmp_symbol(
    pointer_object.type(),
    function_symbol.location,
    function_id,
    function_symbol.mode);

  local_standin_variable = standin_symbol.symbol_expr();

  // Build standin variable initialization block
  init_block.add(
    goto_programt::make_decl(local_standin_variable, function_symbol.location));
  init_block.add(goto_programt::make_assignment(
    code_assignt(local_standin_variable, pointer_object),
    function_symbol.location));
}

std::vector<symbol_exprt>
assigns_clause_scalar_targett::temporary_declarations() const
{
  std::vector<symbol_exprt> result;
  result.push_back(local_standin_variable);
  return result;
}

exprt assigns_clause_scalar_targett::alias_expression(const exprt &ptr)
{
  return same_object(ptr, local_standin_variable);
}

exprt assigns_clause_scalar_targett::compatible_expression(
  const assigns_clause_targett &called_target)
{
  if(called_target.target_type == Scalar)
  {
    return alias_expression(called_target.get_direct_pointer());
  }
  else // Struct or Array
  {
    return false_exprt();
  }
}

goto_programt
assigns_clause_scalar_targett::havoc_code(source_locationt location) const
{
  goto_programt assigns_havoc;

  exprt lhs = dereference_exprt(pointer_object);
  side_effect_expr_nondett rhs(lhs.type(), location);

  goto_programt::targett target =
    assigns_havoc.add(goto_programt::make_assignment(
      code_assignt(std::move(lhs), std::move(rhs)), location));
  target->code_nonconst().add_source_location() = location;

  return assigns_havoc;
}

assigns_clause_struct_targett::assigns_clause_struct_targett(
  const exprt &object_ptr,
  code_contractst &contract,
  messaget &log_parameter,
  const irep_idt &function_id)
  : assigns_clause_targett(
      Struct,
      pointer_for(object_ptr),
      contract,
      log_parameter),
    main_struct_standin(typet())
{
  const symbolt &struct_symbol =
    contract.ns.lookup(to_tag_type(object_ptr.type()));
  const symbolt &function_symbol = contract.ns.lookup(function_id);

  // Declare a new symbol to stand in for the reference
  symbolt struct_temp_symbol = contract.new_tmp_symbol(
    pointer_object.type(),
    function_symbol.location,
    function_id,
    function_symbol.mode);
  main_struct_standin = struct_temp_symbol.symbol_expr();
  local_standin_variables.push_back(main_struct_standin);

  // Build standin variable initialization block
  init_block.add(
    goto_programt::make_decl(main_struct_standin, function_symbol.location));
  init_block.add(goto_programt::make_assignment(
    code_assignt(main_struct_standin, pointer_object),
    function_symbol.location));

  // Handle component members
  std::vector<exprt> component_members;
  const struct_typet &struct_type = to_struct_type(struct_symbol.type);
  for(struct_union_typet::componentt component : struct_type.components())
  {
    exprt current_member = member_exprt(object_ptr, component);
    component_members.push_back(current_member);
  }

  while(!component_members.empty())
  {
    exprt current_operation = component_members.front();
    exprt operation_address = pointer_for(current_operation);

    // Declare a new symbol to stand in for the reference
    symbolt standin_symbol = contract.new_tmp_symbol(
      operation_address.type(),
      function_symbol.location,
      function_id,
      function_symbol.mode);

    symbol_exprt current_standin = standin_symbol.symbol_expr();
    local_standin_variables.push_back(current_standin);

    // Add to standin variable initialization block
    init_block.add(
      goto_programt::make_decl(current_standin, function_symbol.location));
    init_block.add(goto_programt::make_assignment(
      code_assignt(current_standin, operation_address),
      function_symbol.location));

    if(current_operation.type().id() == ID_struct_tag)
    {
      const symbolt &current_struct_symbol =
        contract.ns.lookup(to_tag_type(current_operation.type()));

      const struct_typet &curr_struct_t =
        to_struct_type(current_struct_symbol.type);
      for(struct_union_typet::componentt component : curr_struct_t.components())
      {
        exprt current_member = member_exprt(current_operation, component);
        component_members.push_back(current_member);
      }
    }
    component_members.erase(component_members.begin());
  }
}

std::vector<symbol_exprt>
assigns_clause_struct_targett::temporary_declarations() const
{
  return local_standin_variables;
}

exprt assigns_clause_struct_targett::alias_expression(const exprt &ptr)
{
  exprt::operandst disjuncts;
  disjuncts.reserve(local_standin_variables.size());
  for(symbol_exprt symbol : local_standin_variables)
  {
    const typet &ptr_concrete_type = to_pointer_type(ptr.type()).subtype();
    auto left_size = size_of_expr(ptr_concrete_type, contract.ns);
    const typet &standin_concrete_type =
      to_pointer_type(symbol.type()).subtype();
    auto right_size = size_of_expr(standin_concrete_type, contract.ns);
    INVARIANT(left_size.has_value(), "Unable to determine size of type (lhs).");
    INVARIANT(
      right_size.has_value(), "Unable to determine size of type (rhs).");
    if(*left_size == *right_size)
    {
      exprt same_obj = same_object(ptr, symbol);
      exprt same_offset =
        equal_exprt(pointer_offset(ptr), pointer_offset(symbol));

      disjuncts.push_back(and_exprt{same_obj, same_offset});
    }
  }

  return disjunction(disjuncts);
}

exprt assigns_clause_struct_targett::compatible_expression(
  const assigns_clause_targett &called_target)
{
  if(called_target.target_type == Scalar)
  {
    return alias_expression(called_target.get_direct_pointer());
  }
  else if(called_target.target_type == Struct)
  {
    const assigns_clause_struct_targett &struct_target =
      static_cast<const assigns_clause_struct_targett &>(called_target);

    exprt same_obj =
      same_object(this->main_struct_standin, struct_target.pointer_object);
    // the size of the called struct should be less than or
    // equal to that of the assignable target struct.
    exprt current_size =
      get_size(this->pointer_object.type(), contract.ns, log);
    exprt curr_upper_offset =
      pointer_offset(plus_exprt(this->main_struct_standin, current_size));
    exprt called_size =
      get_size(struct_target.pointer_object.type(), contract.ns, log);
    exprt called_upper_offset =
      pointer_offset(plus_exprt(struct_target.pointer_object, called_size));

    exprt in_range_lower = binary_predicate_exprt(
      pointer_offset(struct_target.pointer_object),
      ID_ge,
      pointer_offset(this->main_struct_standin));
    exprt in_range_upper =
      binary_predicate_exprt(curr_upper_offset, ID_ge, called_upper_offset);

    exprt in_range = and_exprt(in_range_lower, in_range_upper);
    return and_exprt(same_obj, in_range);
  }
  else // Array
  {
    return false_exprt();
  }
}

goto_programt
assigns_clause_struct_targett::havoc_code(source_locationt location) const
{
  goto_programt assigns_havoc;

  exprt lhs = dereference_exprt(pointer_object);
  side_effect_expr_nondett rhs(lhs.type(), location);

  goto_programt::targett target =
    assigns_havoc.add(goto_programt::make_assignment(
      code_assignt(std::move(lhs), std::move(rhs)), location));
  target->code_nonconst().add_source_location() = location;

  return assigns_havoc;
}

assigns_clause_array_targett::assigns_clause_array_targett(
  const exprt &object_ptr,
  code_contractst &contract,
  messaget &log_parameter,
  const irep_idt &function_id)
  : assigns_clause_targett(Array, object_ptr, contract, log_parameter),
    lower_offset_object(),
    upper_offset_object(),
    array_standin_variable(typet()),
    lower_offset_variable(typet()),
    upper_offset_variable(typet())
{
  const symbolt &function_symbol = contract.ns.lookup(function_id);

  // Declare a new symbol to stand in for the reference
  symbolt standin_symbol = contract.new_tmp_symbol(
    pointer_object.type(),
    function_symbol.location,
    function_id,
    function_symbol.mode);

  array_standin_variable = standin_symbol.symbol_expr();

  // Add array temp to variable initialization block
  init_block.add(
    goto_programt::make_decl(array_standin_variable, function_symbol.location));
  init_block.add(goto_programt::make_assignment(
    code_assignt(array_standin_variable, pointer_object),
    function_symbol.location));

  if(object_ptr.id() == ID_address_of)
  {
    exprt constant_size =
      get_size(object_ptr.type().subtype(), contract.ns, log);
    lower_offset_object = typecast_exprt(
      mult_exprt(
        typecast_exprt(object_ptr, unsigned_long_int_type()), constant_size),
      signed_int_type());

    // Declare a new symbol to stand in for the reference
    symbolt lower_standin_symbol = contract.new_tmp_symbol(
      lower_offset_object.type(),
      function_symbol.location,
      function_id,
      function_symbol.mode);

    lower_offset_variable = lower_standin_symbol.symbol_expr();

    // Add array temp to variable initialization block
    init_block.add(goto_programt::make_decl(
      lower_offset_variable, function_symbol.location));
    init_block.add(goto_programt::make_assignment(
      code_assignt(lower_offset_variable, lower_offset_object),
      function_symbol.location));

    upper_offset_object = typecast_exprt(
      mult_exprt(
        typecast_exprt(object_ptr, unsigned_long_int_type()), constant_size),
      signed_int_type());

    // Declare a new symbol to stand in for the reference
    symbolt upper_standin_symbol = contract.new_tmp_symbol(
      upper_offset_object.type(),
      function_symbol.location,
      function_id,
      function_symbol.mode);

    upper_offset_variable = upper_standin_symbol.symbol_expr();

    // Add array temp to variable initialization block
    init_block.add(goto_programt::make_decl(
      upper_offset_variable, function_symbol.location));
    init_block.add(goto_programt::make_assignment(
      code_assignt(upper_offset_variable, upper_offset_object),
      function_symbol.location));
  }
}

std::vector<symbol_exprt>
assigns_clause_array_targett::temporary_declarations() const
{
  std::vector<symbol_exprt> result;
  result.push_back(array_standin_variable);
  result.push_back(lower_offset_variable);
  result.push_back(upper_offset_variable);

  return result;
}

goto_programt
assigns_clause_array_targett::havoc_code(source_locationt location) const
{
  goto_programt assigns_havoc;

  modifiest assigns_tgts;
  typet lower_type = lower_offset_variable.type();
  exprt array_type_size =
    get_size(pointer_object.type().subtype(), contract.ns, log);

  for(mp_integer i = lower_bound; i < upper_bound; ++i)
  {
    irep_idt offset_string(from_integer(i, integer_typet()).get_value());
    irep_idt offset_irep(offset_string);
    constant_exprt val_const(offset_irep, lower_type);
    dereference_exprt array_deref(plus_exprt(
      pointer_object, typecast_exprt(val_const, signed_long_int_type())));

    assigns_tgts.insert(array_deref);
  }

  for(auto lhs : assigns_tgts)
  {
    side_effect_expr_nondett rhs(lhs.type(), location);

    goto_programt::targett target =
      assigns_havoc.add(goto_programt::make_assignment(
        code_assignt(std::move(lhs), std::move(rhs)), location));
    target->code_nonconst().add_source_location() = location;
  }

  return assigns_havoc;
}

exprt assigns_clause_array_targett::alias_expression(const exprt &ptr)
{
  exprt ptr_offset = pointer_offset(ptr);
  exprt::operandst conjuncts;

  conjuncts.push_back(same_object(ptr, array_standin_variable));
  conjuncts.push_back(binary_predicate_exprt(
    ptr_offset,
    ID_ge,
    typecast_exprt(lower_offset_variable, ptr_offset.type())));
  conjuncts.push_back(binary_predicate_exprt(
    typecast_exprt(upper_offset_variable, ptr_offset.type()),
    ID_ge,
    ptr_offset));

  return conjunction(conjuncts);
}

exprt assigns_clause_array_targett::compatible_expression(
  const assigns_clause_targett &called_target)
{
  if(called_target.target_type == Scalar)
  {
    return alias_expression(called_target.get_direct_pointer());
  }
  else if(called_target.target_type == Array)
  {
    const assigns_clause_array_targett &array_target =
      static_cast<const assigns_clause_array_targett &>(called_target);
    exprt same_obj =
      same_object(this->array_standin_variable, array_target.pointer_object);
    exprt in_range_lower = binary_predicate_exprt(
      array_target.lower_offset_object, ID_ge, this->lower_offset_variable);
    exprt in_range_upper = binary_predicate_exprt(
      this->upper_offset_variable, ID_ge, array_target.upper_offset_object);
    exprt in_range = and_exprt(in_range_lower, in_range_upper);
    return and_exprt(same_obj, in_range);
  }
  else // Struct
  {
    return false_exprt();
  }
}

assigns_clauset::assigns_clauset(
  const exprt &assigns,
  code_contractst &contract,
  const irep_idt function_id,
  messaget log_parameter)
  : assigns_expr(assigns),
    parent(contract),
    function_id(function_id),
    log(log_parameter)
{
  for(exprt current_operation : assigns_expr.operands())
  {
    add_target(current_operation);
  }
}
assigns_clauset::~assigns_clauset()
{
  for(assigns_clause_targett *target : targets)
  {
    delete target;
  }
}

assigns_clause_targett *assigns_clauset::add_target(exprt current_operation)
{
  if(current_operation.id() == ID_address_of)
  {
    assigns_clause_array_targett *array_target =
      new assigns_clause_array_targett(
        current_operation, parent, log, function_id);
    targets.push_back(array_target);
    return array_target;
  }
  else if(current_operation.type().id() == ID_struct_tag)
  {
    assigns_clause_struct_targett *struct_target =
      new assigns_clause_struct_targett(
        current_operation, parent, log, function_id);
    targets.push_back(struct_target);
    return struct_target;
  }
  else
  {
    assigns_clause_scalar_targett *scalar_target =
      new assigns_clause_scalar_targett(
        current_operation, parent, log, function_id);
    targets.push_back(scalar_target);
    return scalar_target;
  }
}

assigns_clause_targett *
assigns_clauset::add_pointer_target(exprt current_operation)
{
  return add_target(dereference_exprt(current_operation));
}

goto_programt assigns_clauset::init_block(source_locationt location)
{
  goto_programt result;
  for(assigns_clause_targett *target : targets)
  {
    for(goto_programt::instructiont inst :
        target->get_init_block().instructions)
    {
      result.add(goto_programt::instructiont(inst));
    }
  }
  return result;
}

goto_programt &assigns_clauset::temporary_declarations(
  source_locationt location,
  irep_idt function_name,
  irep_idt language_mode)
{
  if(standin_declarations.empty())
  {
    for(assigns_clause_targett *target : targets)
    {
      for(symbol_exprt symbol : target->temporary_declarations())
      {
        standin_declarations.add(
          goto_programt::make_decl(symbol, symbol.source_location()));
      }
    }
  }
  return standin_declarations;
}

goto_programt assigns_clauset::dead_stmts(
  source_locationt location,
  irep_idt function_name,
  irep_idt language_mode)
{
  goto_programt dead_statements;
  for(assigns_clause_targett *target : targets)
  {
    for(symbol_exprt symbol : target->temporary_declarations())
    {
      dead_statements.add(
        goto_programt::make_dead(symbol, symbol.source_location()));
    }
  }
  return dead_statements;
}

goto_programt assigns_clauset::havoc_code(
  source_locationt location,
  irep_idt function_name,
  irep_idt language_mode)
{
  goto_programt havoc_statements;
  for(assigns_clause_targett *target : targets)
  {
    // (1) If the assigned target is not a dereference,
    // only include the havoc_statement

    // (2) If the assigned target is a dereference, do the following:

    // if(!__CPROVER_w_ok(target, 0)) goto z;
    //      havoc_statements
    // z: skip

    // create the z label
    goto_programt tmp_z;
    goto_programt::targett z = tmp_z.add(goto_programt::make_skip(location));

    const auto &target_ptr = target->get_direct_pointer();
    if(to_address_of_expr(target_ptr).object().id() == ID_dereference)
    {
      // create the condition
      exprt condition =
        not_exprt(w_ok_exprt(target_ptr, from_integer(0, unsigned_int_type())));
      havoc_statements.add(goto_programt::make_goto(z, condition, location));
    }

    // create havoc_statements
    for(goto_programt::instructiont instruction :
        target->havoc_code(location).instructions)
    {
      havoc_statements.add(std::move(instruction));
    }

    if(to_address_of_expr(target_ptr).object().id() == ID_dereference)
    {
      // add the z label instruction
      havoc_statements.destructive_append(tmp_z);
    }
  }
  return havoc_statements;
}

exprt assigns_clauset::alias_expression(const exprt &lhs)
{
  if(targets.empty())
  {
    return false_exprt();
  }

  exprt left_ptr = assigns_clause_targett::pointer_for(lhs);

  bool first_iter = true;
  exprt result = false_exprt();
  for(assigns_clause_targett *target : targets)
  {
    if(first_iter)
    {
      result = target->alias_expression(left_ptr);
      first_iter = false;
    }
    else
    {
      result = or_exprt(result, target->alias_expression(left_ptr));
    }
  }
  return result;
}

exprt assigns_clauset::compatible_expression(
  const assigns_clauset &called_assigns)
{
  if(called_assigns.targets.empty())
  {
    return true_exprt();
  }

  bool first_clause = true;
  exprt result = true_exprt();
  for(assigns_clause_targett *called_target : called_assigns.targets)
  {
    bool first_iter = true;
    exprt current_target_compatible = false_exprt();
    for(assigns_clause_targett *target : targets)
    {
      if(first_iter)
      {
        // TODO: Optimize the validation below and remove code duplication
        // See GitHub issue #6105 for further details

        // Validating the called target through __CPROVER_w_ok() is
        // only useful when the called target is a dereference
        const auto &called_target_ptr = called_target->get_direct_pointer();
        if(
          to_address_of_expr(called_target_ptr).object().id() == ID_dereference)
        {
          // or_exprt is short-circuited, therefore
          // target->compatible_expression(*called_target) would not be
          // checked on invalid called_targets.
          current_target_compatible = or_exprt(
            not_exprt(w_ok_exprt(
              called_target_ptr, from_integer(0, unsigned_int_type()))),
            target->compatible_expression(*called_target));
        }
        else
        {
          current_target_compatible =
            target->compatible_expression(*called_target);
        }
        first_iter = false;
      }
      else
      {
        current_target_compatible = or_exprt(
          current_target_compatible,
          target->compatible_expression(*called_target));
      }
    }
    if(first_clause)
    {
      result = current_target_compatible;
      first_clause = false;
    }
    else
    {
      exprt::operandst conjuncts;
      conjuncts.push_back(result);
      conjuncts.push_back(current_target_compatible);
      result = conjunction(conjuncts);
    }
  }

  return result;
}

std::set<goto_programt::targett> &find_is_fresh_calls_visitort::is_fresh_calls()
{
  return function_set;
}

void find_is_fresh_calls_visitort::clear_set()
{
  function_set.clear();
}

void find_is_fresh_calls_visitort::operator()(goto_programt &prog)
{
  Forall_goto_program_instructions(ins, prog)
  {
    if(ins->is_function_call())
    {
      const code_function_callt &call = ins->get_function_call();

      if(call.function().id() == ID_symbol)
      {
        const irep_idt &fun_name =
          to_symbol_expr(call.function()).get_identifier();

        if(fun_name == (CPROVER_PREFIX + std::string("is_fresh")))
        {
          function_set.insert(ins);
        }
      }
    }
  }
}

void is_fresh_baset::update_requires(goto_programt &requires)
{
  find_is_fresh_calls_visitort requires_visitor;
  requires_visitor(requires);
  for(auto it : requires_visitor.is_fresh_calls())
  {
    create_requires_fn_call(it);
  }
}

void is_fresh_baset::update_ensures(goto_programt &ensures)
{
  find_is_fresh_calls_visitort ensures_visitor;
  ensures_visitor(ensures);
  for(auto it : ensures_visitor.is_fresh_calls())
  {
    create_ensures_fn_call(it);
  }
}

//
//
// Code largely copied from model_argc_argv.cpp
//
//

void is_fresh_baset::add_declarations(const std::string &decl_string)
{
  log.debug() << "Creating declarations: \n" << decl_string << "\n";

  std::istringstream iss(decl_string);

  ansi_c_languaget ansi_c_language;
  ansi_c_language.set_message_handler(log.get_message_handler());
  configt::ansi_ct::preprocessort pp = config.ansi_c.preprocessor;
  config.ansi_c.preprocessor = configt::ansi_ct::preprocessort::NONE;
  ansi_c_language.parse(iss, "");
  config.ansi_c.preprocessor = pp;

  symbol_tablet tmp_symbol_table;
  ansi_c_language.typecheck(tmp_symbol_table, "<built-in-library>");
  exprt value = nil_exprt();

  goto_functionst tmp_functions;

  // Add the new functions into the goto functions table.
  parent.get_goto_functions().function_map[ensures_fn_name].copy_from(
    tmp_functions.function_map[ensures_fn_name]);

  parent.get_goto_functions().function_map[requires_fn_name].copy_from(
    tmp_functions.function_map[requires_fn_name]);

  for(const auto &symbol_pair : tmp_symbol_table.symbols)
  {
    if(
      symbol_pair.first == memmap_name ||
      symbol_pair.first == ensures_fn_name ||
      symbol_pair.first == requires_fn_name || symbol_pair.first == "malloc")
    {
      this->parent.get_symbol_table().insert(symbol_pair.second);
    }
    // Parameters are stored as scoped names in the symbol table.
    else if(
      (has_prefix(
         id2string(symbol_pair.first), id2string(ensures_fn_name) + "::") ||
       has_prefix(
         id2string(symbol_pair.first), id2string(requires_fn_name) + "::")) &&
      parent.get_symbol_table().add(symbol_pair.second))
    {
      UNREACHABLE;
    }
  }

  // We have to set the global memory map array to
  // all zeros for this to work properly
  const array_typet ty =
    to_array_type(tmp_symbol_table.lookup_ref(memmap_name).type);
  constant_exprt initial_value(irep_idt(dstringt("0")), ty.subtype());
  array_of_exprt memmap_init(initial_value, ty);
  goto_programt::instructiont a =
    goto_programt::make_assignment(symbol_exprt(memmap_name, ty), memmap_init);

  // insert the assignment into the initialize function.
  auto called_func =
    parent.get_goto_functions().function_map.find(INITIALIZE_FUNCTION);
  goto_programt &body = called_func->second.body;
  auto target = body.get_end_function();
  body.insert_before(target, a);
}

void is_fresh_baset::update_fn_call(
  goto_programt::targett &ins,
  const std::string &fn_name,
  bool add_address_of)
{
  const code_function_callt &const_call = ins->get_function_call();
  code_function_callt call(
    exprt(const_call.lhs()),
    exprt(const_call.function()),
    code_function_callt::argumentst(const_call.arguments()));

  // adjusting the expression for the first argument, if required
  if(add_address_of)
  {
    INVARIANT(call.arguments().size() > 0, "Function must have arguments");
    call.arguments()[0] = address_of_exprt(call.arguments()[0]);
  }

  // fixing the function name.
  to_symbol_expr(call.function()).set_identifier(fn_name);
  log.debug() << "printing updated call expression: "
              << expr2c(call, parent.get_namespace()) << "\n";

  ins->set_function_call(call);
}

/* Declarations for contract enforcement */

is_fresh_enforcet::is_fresh_enforcet(
  code_contractst &_parent,
  messaget _log,
  irep_idt _fun_id)
  : is_fresh_baset(_parent, _log, _fun_id)
{
  std::stringstream ssreq, ssensure, ssmemmap;
  ssreq << CPROVER_PREFIX << fun_id << "_requires_is_fresh";
  this->requires_fn_name = ssreq.str();

  ssensure << CPROVER_PREFIX << fun_id << "_ensures_is_fresh";
  this->ensures_fn_name = ssensure.str();

  ssmemmap << CPROVER_PREFIX << fun_id << "_memory_map";
  this->memmap_name = ssmemmap.str();
}

void is_fresh_enforcet::create_declarations()
{
  std::ostringstream oss;
  std::string cprover_prefix(CPROVER_PREFIX);
  oss << "static _Bool " << memmap_name
      << "[" + cprover_prefix + "constant_infinity_uint]; \n"
      << "\n"
      << "_Bool " << requires_fn_name
      << "(void **elem, " + cprover_prefix + "size_t size) { \n"
      << "   *elem = malloc(size); \n"
      << "   if (!*elem || " << memmap_name
      << "[" + cprover_prefix + "POINTER_OBJECT(*elem)]) return 0; \n"
      << "   " << memmap_name << "[" + cprover_prefix
      << "POINTER_OBJECT(*elem)] = 1; \n"
      << "   return 1; \n"
      << "} \n"
      << "\n"
      << "_Bool " << ensures_fn_name
      << "(void *elem, " + cprover_prefix + "size_t size) { \n"
      << "   _Bool ok = (!" << memmap_name
      << "[" + cprover_prefix + "POINTER_OBJECT(elem)] && "
      << cprover_prefix + "r_ok(elem, size)); \n"
      << "   " << memmap_name << "[" + cprover_prefix
      << "POINTER_OBJECT(elem)] = 1; \n"
      << "   return ok; \n"
      << "}";

  add_declarations(oss.str());
}

void is_fresh_enforcet::create_requires_fn_call(goto_programt::targett &ins)
{
  update_fn_call(ins, requires_fn_name, true);
}

void is_fresh_enforcet::create_ensures_fn_call(goto_programt::targett &ins)
{
  update_fn_call(ins, ensures_fn_name, false);
}

/* Declarations for contract replacement: note that there may be several
   instances of the same function called in a particular context, so care must be taken
   that the 'call' functions and global data structure are unique for each instance.
   This is why we check that the symbols are unique for each such declaration.  */

std::string unique_symbol(const symbol_tablet &tbl, const std::string &original)
{
  auto size = tbl.next_unused_suffix(original);
  return original + std::to_string(size);
}

is_fresh_replacet::is_fresh_replacet(
  code_contractst &_parent,
  messaget _log,
  irep_idt _fun_id)
  : is_fresh_baset(_parent, _log, _fun_id)
{
  std::stringstream ssreq, ssensure, ssmemmap;
  ssreq /* << CPROVER_PREFIX */ << fun_id << "_call_requires_is_fresh";
  this->requires_fn_name =
    unique_symbol(parent.get_symbol_table(), ssreq.str());

  ssensure /* << CPROVER_PREFIX */ << fun_id << "_call_ensures_is_fresh";
  this->ensures_fn_name =
    unique_symbol(parent.get_symbol_table(), ssensure.str());

  ssmemmap /* << CPROVER_PREFIX */ << fun_id << "_memory_map";
  this->memmap_name = unique_symbol(parent.get_symbol_table(), ssmemmap.str());
}

void is_fresh_replacet::create_declarations()
{
  std::ostringstream oss;
  std::string cprover_prefix(CPROVER_PREFIX);
  oss << "static _Bool " << memmap_name
      << "[" + cprover_prefix + "constant_infinity_uint]; \n"
      << "\n"
      << "static _Bool " << requires_fn_name
      << "(void *elem, " + cprover_prefix + "size_t size) { \n"
      << "  _Bool r_ok = " + cprover_prefix + "r_ok(elem, size); \n"
      << "  if (" << memmap_name
      << "[" + cprover_prefix + "POINTER_OBJECT(elem)]"
      << " != 0 || !r_ok)  return 0; \n"
      << "  " << memmap_name << "["
      << cprover_prefix + "POINTER_OBJECT(elem)] = 1; \n"
      << "  return 1; \n"
      << "} \n"
      << " \n"
      << "_Bool " << ensures_fn_name
      << "(void **elem, " + cprover_prefix + "size_t size) { \n"
      << "  *elem = malloc(size); \n"
      << "  return (*elem != 0); \n"
      << "} \n";

  add_declarations(oss.str());
}

void is_fresh_replacet::create_requires_fn_call(goto_programt::targett &ins)
{
  update_fn_call(ins, requires_fn_name, false);
}

void is_fresh_replacet::create_ensures_fn_call(goto_programt::targett &ins)
{
  update_fn_call(ins, ensures_fn_name, true);
}
