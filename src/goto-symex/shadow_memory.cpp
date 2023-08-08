/*******************************************************************\

Module: Symex Shadow Memory Instrumentation

Author: Peter Schrammel

\*******************************************************************/

/// \file
/// Symex Shadow Memory Instrumentation

#include "shadow_memory.h"

#include <util/bitvector_types.h>
#include <util/c_types.h>
#include <util/expr_initializer.h>
#include <util/format_expr.h>
#include <util/format_type.h>
#include <util/fresh_symbol.h>
#include <util/pointer_expr.h>
#include <util/prefix.h>
#include <util/string_constant.h>

#include <langapi/language_util.h>
#include <linking/static_lifetime_init.h>
#include <pointer-analysis/value_set_dereference.h>

#include "goto_symex_state.h"
#include "shadow_memory_util.h"

// TODO: doxygen?
static exprt
build_if_else_expr(std::vector<std::pair<exprt, exprt>> conds_values)
{
  DATA_INVARIANT(
    !conds_values.empty(), "build_if_else_expr requires non-empty argument");
  exprt result = nil_exprt();
  for(const auto &cond_value : conds_values)
  {
    if(result.is_nil())
    {
      result = cond_value.second;
    }
    else
    {
      result = if_exprt(cond_value.first, cond_value.second, result);
    }
  }
  return result;
}

// TODO: doxygen?
static bool
are_types_compatible(const typet &expr_type, const typet &shadow_type)
{
  if(expr_type.id() != ID_pointer || shadow_type.id() != ID_pointer)
    return true;

  // We filter out the particularly annoying case of char ** being definitely
  // incompatible with char[].
  const typet &expr_subtype = to_pointer_type(expr_type).base_type();
  const typet &shadow_subtype = to_pointer_type(shadow_type).base_type();

  if(
    expr_subtype.id() == ID_pointer &&
    to_pointer_type(expr_subtype).base_type().id() != ID_pointer &&
    shadow_subtype.id() == ID_array &&
    to_array_type(shadow_subtype).element_type().id() != ID_pointer)
  {
    return false;
  }
  if(
    shadow_subtype.id() == ID_pointer &&
    to_pointer_type(shadow_subtype).base_type().id() != ID_pointer &&
    expr_subtype.id() != ID_pointer)
  {
    return false;
  }
  return true;
}

// TODO: doxygen?
/// We simplify &string_constant[0] to &string_constant to facilitate expression
/// equality for exact matching.
static void clean_string_constant(exprt &expr)
{
  if(
    expr.id() == ID_index && to_index_expr(expr).index().is_zero() &&
    to_index_expr(expr).array().id() == ID_string_constant)
  {
    expr = to_index_expr(expr).array();
  }
}

// TODO: doxygen?
static void adjust_array_types(typet &type)
{
  if(type.id() != ID_pointer)
  {
    return;
  }
  const typet &subtype = to_pointer_type(type).base_type();
  if(subtype.id() == ID_array)
  {
    to_pointer_type(type).base_type() = to_array_type(subtype).element_type();
  }
  if(subtype.id() == ID_string_constant)
  {
    to_pointer_type(type).base_type() = char_type();
  }
}

// TODO: doxygen?
static exprt get_matched_base_cond(
  const exprt &shadowed_address,
  const exprt &matched_base_address,
  const namespacet &ns,
  const messaget &log)
{
  typet shadowed_address_type = shadowed_address.type();
  adjust_array_types(shadowed_address_type);
  exprt lhs =
    typecast_exprt::conditional_cast(shadowed_address, shadowed_address_type);
  exprt rhs = typecast_exprt::conditional_cast(
    matched_base_address, shadowed_address_type);
  exprt base_cond = simplify_expr(equal_exprt(lhs, rhs), ns);
  if(
    base_cond.id() == ID_equal &&
    to_equal_expr(base_cond).lhs() == to_equal_expr(base_cond).rhs())
  {
    return true_exprt();
  }
  if(base_cond.id() == ID_equal)
  {
    const equal_exprt &equal_expr = to_equal_expr(base_cond);
    const bool equality_types_match =
      equal_expr.lhs().type() == equal_expr.rhs().type();
    DATA_INVARIANT_WITH_DIAGNOSTICS(
      equality_types_match,
      "types of expressions on each side of equality should match",
      irep_pretty_diagnosticst{equal_expr.lhs()},
      irep_pretty_diagnosticst{equal_expr.rhs()});
  }

  return base_cond;
}

// TODO: doxygen?
static exprt get_matched_expr_cond(
  const exprt &dereference_pointer,
  const exprt &expr,
  const namespacet &ns,
  const messaget &log)
{
  typet expr_type = expr.type();
  adjust_array_types(expr_type);
  exprt expr_cond = simplify_expr(
    equal_exprt(
      typecast_exprt::conditional_cast(expr, expr_type),
      typecast_exprt::conditional_cast(dereference_pointer, expr_type)),
    ns);
  if(
    expr_cond.id() == ID_equal &&
    to_equal_expr(expr_cond).lhs() == to_equal_expr(expr_cond).rhs())
  {
    return true_exprt();
  }
  if(expr_cond.id() == ID_equal)
  {
    const equal_exprt &equal_expr = to_equal_expr(expr_cond);
    const bool equality_types_match =
      equal_expr.lhs().type() == equal_expr.rhs().type();
    DATA_INVARIANT_WITH_DIAGNOSTICS(
      equality_types_match,
      "types of expressions on each side of equality should match",
      irep_pretty_diagnosticst{equal_expr.lhs()},
      irep_pretty_diagnosticst{equal_expr.rhs()});
  }
  return expr_cond;
}

void shadow_memoryt::initialize_shadow_memory(
  goto_symex_statet &state,
  exprt expr,
  const shadow_memory_field_definitionst::field_definitiont &fields)
{
  typet type = ns.follow(expr.type());
  clean_pointer_expr(expr, type);
  for(const auto &field_pair : fields)
  {
    const symbol_exprt &shadow = add_field(state, expr, field_pair.first, type);

    if(
      field_pair.second.id() == ID_typecast &&
      to_typecast_expr(field_pair.second).op().is_zero())
    {
      const auto zero_value =
        zero_initializer(type, expr.source_location(), ns);
      CHECK_RETURN(zero_value.has_value());

      symex_assign(state, shadow, *zero_value);
    }
    else
    {
      exprt init_expr = field_pair.second;
      const auto init_value =
        expr_initializer(type, expr.source_location(), ns, init_expr);
      CHECK_RETURN(init_value.has_value());

      symex_assign(state, shadow, *init_value);
    }

    log.debug() << "Shadow memory: initialize field "
                << id2string(shadow.get_identifier()) << " for " << format(expr)
                << " with initial value " << format(field_pair.second)
                << messaget::eom;
  }
}

const symbol_exprt &shadow_memoryt::add_field(
  goto_symex_statet &state,
  const exprt &expr,
  const irep_idt &field_name,
  const typet &field_type)
{
  const auto &function_symbol = ns.lookup(state.source.function_id);
  const symbolt &new_symbol = get_fresh_aux_symbol(
    field_type,
    id2string(state.source.function_id),
    SHADOW_MEMORY_PREFIX + from_expr(expr) + "__" + id2string(field_name),
    state.source.pc->source_location(),
    function_symbol.mode,
    state.symbol_table);

  auto &addresses = state.shadow_memory.address_fields[field_name];
  addresses.push_back(
    shadow_memory_statet::shadowed_addresst{expr, new_symbol.symbol_expr()});

  return addresses.back().shadow;
}

void shadow_memoryt::symex_set_field(
  goto_symex_statet &state,
  const exprt::operandst &arguments)
{
  // To be implemented
}

// TODO: doxygen?
static value_set_dereferencet::valuet get_shadow(
  const exprt &shadow,
  const object_descriptor_exprt &matched_base_descriptor,
  const exprt &expr,
  const namespacet &ns,
  const messaget &log)
{
  object_descriptor_exprt shadowed_object = matched_base_descriptor;
  shadowed_object.object() = shadow;
  value_set_dereferencet::valuet shadow_dereference =
    value_set_dereferencet::build_reference_to(shadowed_object, expr, ns);
#ifdef DEBUG_SM
  log.debug() << "Shadow memory: shadow-deref: "
              << format(shadow_dereference.value) << messaget::eom;
#endif
  return shadow_dereference;
}

// TODO: doxygen?
// foreach shadowed_address in SM:
//   if(incompatible(shadow.object, object)) continue; // Type incompatibility
//   base_match = object == shadow_object; // Do the base obj match the SM obj
//   if(!base_match) continue;
//   if(object is null) continue; // Done in the caller
//   if(type_of(dereference object is void)
//     // we terminate as we don't know how big is the memory at that location
//   shadowed_dereference.pointer = deref(shadow.shadowed_object, expr);
//   aggregate the object depending on the tyep
//   expr_match = shadowed_dereference == expr
//   if(!expr_match) continue;
//   shadow_dereference = deref(shadow.object, expr);
//   if(expr_match)
//     result = shadow_dereference.value [exact match]
//     break;
//   if(base_match) [shadow memory matches]
//     result += (expr_match,  shadow_dereference.value)
//     break;
//   result += (base_match & expr_match,  shadow_dereference.value)
static std::vector<std::pair<exprt, exprt>> get_field(
  const namespacet &ns,
  const messaget &log,
  const exprt &matched_object,
  const std::vector<shadow_memory_statet::shadowed_addresst> &addresses,
  const typet &field_type,
  const exprt &expr,
  const typet &lhs_type,
  bool &exact_match)
{
  std::vector<std::pair<exprt, exprt>> result;

  for(const auto &shadowed_address : addresses)
  {
    log_try_shadow_address(ns, log, shadowed_address);
    if(!are_types_compatible(expr.type(), shadowed_address.address.type()))
    {
#ifdef DEBUG_SM
      log.debug() << "Shadow memory: incompatible types "
                  << from_type(ns, "", expr.type()) << ", "
                  << from_type(ns, "", shadowed_address.address.type())
                  << messaget::eom;
#endif
      continue;
    }
    const object_descriptor_exprt &matched_base_descriptor =
      to_object_descriptor_expr(matched_object);
    exprt matched_base_address =
      address_of_exprt(matched_base_descriptor.object());
    clean_string_constant(to_address_of_expr(matched_base_address).op());

    // NULL has already been handled in the caller; skip.
    if(matched_base_descriptor.object().id() == ID_null_object)
    {
      continue;
    }

    // Used only to check if the pointer is `void`
    value_set_dereferencet::valuet dereference =
      value_set_dereferencet::build_reference_to(matched_object, expr, ns);

    if(is_void_pointer(dereference.pointer.type()))
    {
      std::stringstream s;
      s << "Shadow memory: cannot get shadow memory via type void* for "
        << format(expr)
        << ". Insert a cast to the type that you want to access.";
      throw invalid_input_exceptiont(s.str());
    }

    // Replace original memory with the shadow object (symbolic dereferencing is
    // performed during symex later on).
    const value_set_dereferencet::valuet shadow_dereference = get_shadow(
      shadowed_address.shadow, matched_base_descriptor, expr, ns, log);
    log_value_set_match(
      ns,
      log,
      shadowed_address,
      matched_base_address,
      dereference,
      expr,
      shadow_dereference);

    const bool is_union = matched_base_descriptor.type().id() == ID_union ||
                          matched_base_descriptor.type().id() == ID_union_tag;

    exprt value;
    // Aggregate the value depending on the type
    if(field_type.id() == ID_c_bool || field_type.id() == ID_bool)
    {
      // Value is of bool type, so aggregate with or.
      value = typecast_exprt::conditional_cast(
        compute_or_over_cells(
          shadow_dereference.value, field_type, ns, log, is_union),
        lhs_type);
    }
    else
    {
      // Value is of other (bitvector) type, so aggregate with max
      value = typecast_exprt::conditional_cast(
        compute_max_over_cells(
          shadow_dereference.value, field_type, ns, log, is_union),
        lhs_type);
    }

    const exprt base_cond = get_matched_base_cond(
      shadowed_address.address, matched_base_address, ns, log);
    log_cond(ns, log, "base_cond", base_cond);
    if(base_cond.is_false())
    {
      continue;
    }

    const exprt expr_cond =
      get_matched_expr_cond(dereference.pointer, expr, ns, log);
    if(expr_cond.is_false())
    {
      continue;
    }

    if(base_cond.is_true() && expr_cond.is_true())
    {
#ifdef DEBUG_SM
      log.debug() << "exact match" << messaget::eom;
#endif
      exact_match = true;
      result.clear();
      result.emplace_back(base_cond, value);
      break;
    }

    if(base_cond.is_true())
    {
      // No point looking at further shadow addresses
      // as only one of them can match.
#ifdef DEBUG_SM
      log.debug() << "base match" << messaget::eom;
#endif
      result.clear();
      result.emplace_back(expr_cond, value);
      break;
    }
    else
    {
#ifdef DEBUG_SM
      log.debug() << "conditional match" << messaget::eom;
#endif
      result.emplace_back(and_exprt(base_cond, expr_cond), value);
    }
  }
  return result;
}

// Function synopsis
// value_set = get_value_set(expr)
// foreach object in value_set:
//   if(object invalid) continue;
//  get_field(&exact_match)
//  if(exact_match)
//    return;
// return;
void shadow_memoryt::symex_get_field(
  goto_symex_statet &state,
  const exprt &lhs,
  const exprt::operandst &arguments)
{
  INVARIANT(
    arguments.size() == 2, CPROVER_PREFIX "get_field requires 2 arguments");
  irep_idt field_name = extract_field_name(arguments[1]);

  exprt expr = arguments[0];
  typet expr_type = expr.type();
  DATA_INVARIANT(
    expr_type.id() == ID_pointer,
    "shadow memory requires a pointer expression");
  log_get_field(ns, log, field_name, expr);

  INVARIANT(
    state.shadow_memory.address_fields.count(field_name) == 1,
    id2string(field_name) + " should exist");
  const auto &addresses = state.shadow_memory.address_fields.at(field_name);

  // Return null (invalid object) instead of auto-object or invalid-object.
  // This will "polish" expr from invalid and auto-obj
  replace_invalid_object_by_null(expr);

  std::vector<exprt> value_set = state.value_set.get_value_set(expr, ns);
  log_value_set(ns, log, value_set);

  std::vector<std::pair<exprt, exprt>> rhs_conds_values;
  const null_pointer_exprt null_pointer(to_pointer_type(expr.type()));
  // Used to give a default value for invalid pointers and other usages
  const exprt &field_init_expr = get_field_init_expr(field_name, state);

  if(contains_compatible_address(value_set, null_pointer))
  {
    log_value_set_match(ns, log, null_pointer, expr);
    // If we have an invalid pointer, then return the default value of the
    // shadow memory as dereferencing it would fail
    rhs_conds_values.emplace_back(
      true_exprt(),
      typecast_exprt::conditional_cast(field_init_expr, lhs.type()));
  }

  for(const auto &matched_object : value_set)
  {
    // Ignore "unknown"
    if(matched_object.id() != ID_object_descriptor)
    {
      log.warning() << "Shadow memory: value set contains unknown for "
                    << format(expr) << messaget::eom;
      continue;
    }
    // Ignore "integer_address"
    if(
      to_object_descriptor_expr(matched_object).root_object().id() ==
      ID_integer_address)
    {
      log.warning() << "Shadow memory: value set contains integer_address for "
                    << format(expr) << messaget::eom;
      continue;
    }
    // Ignore "ID_C_is_failed_symbol" (another incarnation of invalid pointer)
    // TODO: check if this is obsolete (or removed by
    //  replace_invalid_object_by_null) or add default value
    if(matched_object.type().get_bool(ID_C_is_failed_symbol))
    {
      log.warning() << "Shadow memory: value set contains failed symbol for "
                    << format(expr) << messaget::eom;
      continue;
    }

    bool exact_match = false;

    // List of condition ==> value (read condition implies values)
    auto per_matched_object_conds_values = get_field(
      ns,
      log,
      matched_object,
      addresses,
      field_init_expr.type(),
      expr,
      lhs.type(),
      exact_match);

    // If we have an exact match we discard all the previous conditions and
    // create an assignment. Then we'll break
    if(exact_match)
    {
      rhs_conds_values.clear();
    }
    // Process this match (exact will contain only one set of conditions).
    rhs_conds_values.insert(
      rhs_conds_values.end(),
      per_matched_object_conds_values.begin(),
      per_matched_object_conds_values.end());
    if(exact_match)
    {
      break;
    }
  }

  // If we have at least a condition ==> value
  if(!rhs_conds_values.empty())
  {
    // Build the rhs expression from the shadow memory (big switch for all
    // condition ==> value)
    exprt rhs = typecast_exprt::conditional_cast(
      build_if_else_expr(rhs_conds_values), lhs.type());
    const size_t mux_size = rhs_conds_values.size() - 1;
    // Don't debug if the switch is too big
    if(mux_size >= 10)
    {
      log.warning() << "Shadow memory: mux size get_field: " << mux_size
                    << messaget::eom;
    }
    else
    {
      log.debug() << "Shadow memory: mux size get_field: " << mux_size
                  << messaget::eom;
    }
#ifdef DEBUG_SM
    log.debug() << "Shadow memory: RHS: " << format(rhs) << messaget::eom;
#endif
    // TODO: create the assignment of __CPROVER_shadow_memory_get_field
    symex_assign(state, lhs, typecast_exprt::conditional_cast(rhs, lhs.type()));
  }
  else
  {
    // We don't have any condition ==> value for the pointer, so we fall-back to
    // the initialization value of the shadow-memory.
    log.warning() << "Shadow memory: cannot get_field for " << format(expr)
                  << messaget::eom;
    symex_assign(
      state,
      lhs,
      typecast_exprt::conditional_cast(field_init_expr, lhs.type()));
  }
}

// TODO: the following 4 functions (`symex_field_static_init`,
//                                  `symex_field_static_init_string_constant`,
//                                  `symex_field_local_init`,
//                                  `symex_field_dynamic_init`) do filtering on
//  the input symbol name and then call `initialize_shadow_memory` accordingly.
//  We want to refactor and improve the way the filtering is done, but given
//  that we don't have an easy mechanism to validate that we haven't changed the
//  behaviour, we want to postpone changing this until the full shadow memory
//  functionalities are integrated and we have good regression/unit testing.

void shadow_memoryt::symex_field_static_init(
  goto_symex_statet &state,
  const ssa_exprt &lhs)
{
  if(lhs.get_original_expr().id() != ID_symbol)
    return;

  const irep_idt &identifier =
    to_symbol_expr(lhs.get_original_expr()).get_identifier();

  if(state.source.function_id != INITIALIZE_FUNCTION)
    return;

  if(
    has_prefix(id2string(identifier), CPROVER_PREFIX) &&
    !has_prefix(id2string(identifier), CPROVER_PREFIX "errno"))
  {
    return;
  }

  const symbolt &symbol = ns.lookup(identifier);

  if(
    (id2string(symbol.name).find("::return_value") == std::string::npos &&
     symbol.is_auxiliary) ||
    !symbol.is_static_lifetime)
    return;
  if(id2string(symbol.name).find("__cs_") != std::string::npos)
    return;

  const typet &type = symbol.type;
  log.debug() << "Shadow memory: global memory " << id2string(identifier)
              << " of type " << from_type(ns, "", type) << messaget::eom;

  initialize_shadow_memory(
    state, lhs, state.shadow_memory.fields.global_fields);
}

void shadow_memoryt::symex_field_static_init_string_constant(
  goto_symex_statet &state,
  const ssa_exprt &expr,
  const exprt &rhs)
{
  if(
    expr.get_original_expr().id() == ID_symbol &&
    has_prefix(
      id2string(to_symbol_expr(expr.get_original_expr()).get_identifier()),
      CPROVER_PREFIX))
  {
    return;
  }
  const index_exprt &index_expr =
    to_index_expr(to_address_of_expr(rhs).object());

  const typet &type = index_expr.array().type();
  log.debug() << "Shadow memory: global memory "
              << id2string(to_string_constant(index_expr.array()).get_value())
              << " of type " << from_type(ns, "", type) << messaget::eom;

  initialize_shadow_memory(
    state, index_expr.array(), state.shadow_memory.fields.global_fields);
}

void shadow_memoryt::symex_field_local_init(
  goto_symex_statet &state,
  const ssa_exprt &expr)
{
  const symbolt &symbol =
    ns.lookup(to_symbol_expr(expr.get_original_expr()).get_identifier());

  const std::string symbol_name = id2string(symbol.name);
  if(
    symbol.is_auxiliary &&
    symbol_name.find("::return_value") == std::string::npos)
    return;
  if(
    symbol_name.find("malloc::") != std::string::npos &&
    (symbol_name.find("malloc_size") != std::string::npos ||
     symbol_name.find("malloc_res") != std::string::npos ||
     symbol_name.find("record_malloc") != std::string::npos ||
     symbol_name.find("record_may_leak") != std::string::npos))
  {
    return;
  }
  if(
    symbol_name.find("__builtin_alloca::") != std::string::npos &&
    (symbol_name.find("alloca_size") != std::string::npos ||
     symbol_name.find("record_malloc") != std::string::npos ||
     symbol_name.find("record_alloca") != std::string::npos ||
     symbol_name.find("res") != std::string::npos))
  {
    return;
  }
  if(symbol_name.find("__cs_") != std::string::npos)
    return;

  const typet &type = expr.type();
  ssa_exprt expr_l1 = remove_level_2(expr);
  log.debug() << "Shadow memory: local memory "
              << id2string(expr_l1.get_identifier()) << " of type "
              << from_type(ns, "", type) << messaget::eom;

  initialize_shadow_memory(
    state, expr_l1, state.shadow_memory.fields.local_fields);
}

void shadow_memoryt::symex_field_dynamic_init(
  goto_symex_statet &state,
  const exprt &expr,
  const side_effect_exprt &code)
{
  log.debug() << "Shadow memory: dynamic memory of type "
              << from_type(ns, "", expr.type()) << messaget::eom;

  initialize_shadow_memory(
    state, expr, state.shadow_memory.fields.global_fields);
}

shadow_memory_field_definitionst shadow_memoryt::gather_field_declarations(
  const abstract_goto_modelt &goto_model,
  message_handlert &message_handler)
{
  shadow_memory_field_definitionst field_definitions;

  // Gather shadow memory declarations from goto model
  for(const auto &goto_function : goto_model.get_goto_functions().function_map)
  {
    const auto &goto_program = goto_function.second.body;
    forall_goto_program_instructions(target, goto_program)
    {
      if(!target->is_function_call())
        continue;

      const auto &code_function_call = to_code_function_call(target->code());
      const exprt &function = code_function_call.function();

      if(function.id() != ID_symbol)
        continue;

      const irep_idt &identifier = to_symbol_expr(function).get_identifier();

      if(
        identifier ==
        CPROVER_PREFIX SHADOW_MEMORY_FIELD_DECL SHADOW_MEMORY_GLOBAL_SCOPE)
      {
        convert_field_declaration(
          code_function_call,
          field_definitions.global_fields,
          true,
          message_handler);
      }
      else if(
        identifier ==
        CPROVER_PREFIX SHADOW_MEMORY_FIELD_DECL SHADOW_MEMORY_LOCAL_SCOPE)
      {
        convert_field_declaration(
          code_function_call,
          field_definitions.local_fields,
          false,
          message_handler);
      }
    }
  }
  return field_definitions;
}

void shadow_memoryt::convert_field_declaration(
  const code_function_callt &code_function_call,
  shadow_memory_field_definitionst::field_definitiont &fields,
  bool is_global,
  message_handlert &message_handler)
{
  INVARIANT(
    code_function_call.arguments().size() == 2,
    std::string(CPROVER_PREFIX) + SHADOW_MEMORY_FIELD_DECL +
      (is_global ? SHADOW_MEMORY_GLOBAL_SCOPE : SHADOW_MEMORY_LOCAL_SCOPE) +
      " requires 2 arguments");
  irep_idt field_name = extract_field_name(code_function_call.arguments()[0]);

  exprt expr = code_function_call.arguments()[1];

  messaget log(message_handler);
  log.debug() << "Shadow memory: declare " << (is_global ? "global " : "local ")
              << "field " << id2string(field_name) << " of type "
              << format(expr.type()) << messaget::eom;
  if(!can_cast_type<bitvector_typet>(expr.type()))
  {
    throw unsupported_operation_exceptiont(
      "A shadow memory field must be of a bitvector type.");
  }
  if(to_bitvector_type(expr.type()).get_width() > 8)
  {
    throw unsupported_operation_exceptiont(
      "A shadow memory field must not be larger than 8 bits.");
  }

  // record the field's initial value (and type)
  fields[field_name] = expr;
}
