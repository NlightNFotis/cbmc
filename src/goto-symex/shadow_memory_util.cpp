/*******************************************************************\

Module: Symex Shadow Memory Instrumentation

Author: Peter Schrammel

\*******************************************************************/

/// \file
/// Symex Shadow Memory Instrumentation Utilities

#include "shadow_memory_util.h"

#include <util/arith_tools.h>
#include <util/c_types.h>
#include <util/format_expr.h>
#include <util/invariant.h>
#include <util/namespace.h>
#include <util/pointer_expr.h>
#include <util/ssa_expr.h>
#include <util/std_expr.h>

#include <util/bitvector_expr.h>
#include <util/byte_operators.h>

irep_idt extract_field_name(const exprt &string_expr)
{
  if(string_expr.id() == ID_typecast)
    return extract_field_name(to_typecast_expr(string_expr).op());
  else if(string_expr.id() == ID_address_of)
    return extract_field_name(to_address_of_expr(string_expr).object());
  else if(string_expr.id() == ID_index)
    return extract_field_name(to_index_expr(string_expr).array());
  else if(string_expr.id() == ID_string_constant)
  {
    return string_expr.get(ID_value);
  }
  else
    UNREACHABLE;
}

/// If the given type is an array type, then remove the L2 level
/// renaming from its size.
/// \param type Type to be modified
static void remove_array_type_l2(typet &type)
{
  if(to_array_type(type).size().id() != ID_symbol)
    return;

  ssa_exprt &size = to_ssa_expr(to_array_type(type).size());
  size.remove_level_2();
}

void clean_pointer_expr(exprt &expr, const typet &type)
{
  if(
    type.id() == ID_array && expr.id() == ID_symbol &&
    to_array_type(type).size().get_bool(ID_C_SSA_symbol))
  {
    remove_array_type_l2(expr.type());
    exprt original_expr = to_ssa_expr(expr).get_original_expr();
    remove_array_type_l2(original_expr.type());
    to_ssa_expr(expr).set_expression(original_expr);
  }
  if(expr.id() == ID_string_constant)
  {
    expr = address_of_exprt(expr);
    expr.type() = pointer_type(char_type());
  }
  else if(expr.id() == ID_dereference)
  {
    expr = to_dereference_expr(expr).pointer();
  }
  else
  {
    expr = address_of_exprt(expr);
  }
  POSTCONDITION(expr.type().id() == ID_pointer);
}

void log_get_field(
  const namespacet &ns,
  const messaget &log,
  const irep_idt &field_name,
  const exprt &expr)
{
  log.conditional_output(
    log.debug(), [ns, field_name, expr](messaget::mstreamt &mstream) {
      mstream << "Shadow memory: get_field: " << id2string(field_name) << " for "
              << format(expr) << messaget::eom;
    });
}

void log_cond(
  const namespacet &ns,
  const messaget &log,
  const char *cond_text,
  const exprt &cond)
{
#ifdef DEBUG_SM
  log.conditional_output(
    log.debug(), [ns, cond_text, cond](messaget::mstreamt &mstream) {
      mstream << "Shadow memory: " << cond_text << ": " << format(cond)
              << messaget::eom;
    });
#endif
}

void replace_invalid_object_by_null(exprt &expr)
{
  if(expr.id() == ID_symbol && expr.type().id() == ID_pointer &&
     (id2string(to_symbol_expr(expr).get_identifier()).rfind("invalid_object") != std::string::npos ||
      id2string(to_symbol_expr(expr).get_identifier()).rfind("$object") != std::string::npos))
  {
    expr = null_pointer_exprt(to_pointer_type(expr.type()));
    return;
  }
  Forall_operands(it, expr)
  {
    replace_invalid_object_by_null(*it);
  }
}

void log_value_set(
  const namespacet &ns,
  const messaget &log,
  const std::vector<exprt> &value_set)
{
#ifdef DEBUG_SM
  log.conditional_output(
    log.debug(), [ns, value_set](messaget::mstreamt &mstream) {
      for(const auto &e : value_set)
      {
        mstream << "Shadow memory: value set: " << format(e) << messaget::eom;
      }
    });
#endif
}

const exprt &get_field_init_expr(
  const irep_idt& field_name,
  const goto_symex_statet &state)
{
  auto field_type_it = state.shadow_memory.fields.local_fields.find(field_name);
  if (field_type_it != state.shadow_memory.fields.local_fields.end()) {
    return field_type_it->second;
  }
  field_type_it = state.shadow_memory.fields.global_fields.find(field_name);
  CHECK_RETURN(field_type_it != state.shadow_memory.fields.global_fields.end());
  return field_type_it->second;
}

/// Given a pointer expression `address` checks if any expr in value_set is compatible
bool filter_by_value_set(
  const std::vector<exprt>  &value_set,
  const exprt &address)
{
  //log.debug() << "address: " << address.pretty() << messaget::eom;

  if(
    address.id() == ID_constant && address.type().id() == ID_pointer &&
    to_constant_expr(address).is_zero())
  {
    for(const auto &e : value_set)
    {
      if(e.id() != ID_object_descriptor)
        continue;

      const exprt &expr = to_object_descriptor_expr(e).object();
      if(expr.id() == ID_null_object)
        return true;
    }
    return false;
  }

  for(const auto &e : value_set)
  {
    if(e.id() == ID_unknown || e.id() == ID_object_descriptor)
      return true;
  }
  return false;
}



void log_value_set_match(
  const namespacet &ns,
  const messaget &log,
  const shadow_memory_statet::shadowed_addresst &shadowed_address,
  const exprt &matched_base_address,
  const value_set_dereferencet::valuet &dereference,
  const exprt &expr,
  const value_set_dereferencet::valuet &shadow_dereference)
{
#ifdef DEBUG_SM
  log.conditional_output(
    log.debug(),
    [ns, shadowed_address, expr, dereference, matched_base_address, shadow_dereference](
      messaget::mstreamt &mstream) {
      mstream << "Shadow memory: value set match: " << messaget::eom;
      mstream << "Shadow memory:   base: " << format(shadowed_address.address)
              << " <-- " << format(matched_base_address) << messaget::eom;
      mstream << "Shadow memory:   cell: " << format(dereference.pointer) << " <-- "
              << format(expr) << messaget::eom;
      mstream << "Shadow memory:   shadow ptr: "
              << format(shadow_dereference.pointer) << messaget::eom;
      mstream << "Shadow memory:   shadow val: "
              << format(shadow_dereference.value) << messaget::eom;
    });
#endif
}

void log_value_set_match(
  const namespacet &ns,
  const messaget &log,
  const exprt &address,
  const exprt &expr)
{
  // Leave guards rename to DEBUG_SHADOW_MEMORY
#ifdef DEBUG_SM
  log.conditional_output(
    log.debug(), [ns, address, expr](messaget::mstreamt &mstream) {
      mstream << "Shadow memory: value set match: " << format(address)
              << " <-- " << format(expr) << messaget::eom;
    });
#endif
}

void log_try_shadow_address(
  const namespacet &ns,
  const messaget &log,
  const shadow_memory_statet::shadowed_addresst &shadowed_address)
{
#ifdef DEBUG_SM
  log.conditional_output(
    log.debug(), [ns, shadowed_address](messaget::mstreamt &mstream) {
      mstream << "Shadow memory: trying shadowed address: "
              << format(shadowed_address.address)
              << messaget::eom;
    });
#endif
}

// TODO: doxygen?
static exprt conditional_cast_floatbv_to_unsignedbv(const exprt &value)
{
  if(value.type().id() != ID_floatbv)
  {
    return value;
  }
  return make_byte_extract(
    value,
    from_integer(0, unsigned_int_type()),
    unsignedbv_typet(to_bitvector_type(value.type()).get_width()));
}

// TODO: doxygen?
static void or_over_bytes(
  const exprt &value,
  const typet &type,
  const typet &field_type,
  exprt::operandst &values)
{
  const size_t size = to_bitvector_type(type).get_width() / 8;
  values.push_back(typecast_exprt::conditional_cast(value, field_type));
  for(size_t i = 1; i < size; ++i)
  {
    values.push_back(
      typecast_exprt::conditional_cast(
        lshr_exprt(value, from_integer(8 * i, size_type())),
        field_type));
  }
}

// TODO: doxygen?
static void or_elements(
  exprt element,
  const typet &field_type,
  const namespacet &ns,
  const messaget &log,
  const bool is_union,
  exprt::operandst &values)
{
  element = conditional_cast_floatbv_to_unsignedbv(element);
  if(element.type().id() == ID_unsignedbv || element.type().id() == ID_signedbv)
  {
    exprt value = element;
    if(is_union)
    {
      or_over_bytes(value, element.type(), field_type, values);
    }
    else
    {
      values.push_back(typecast_exprt::conditional_cast(value, field_type));
    }
  }
  else
  {
    exprt value = compute_or_over_cells(element,
                                        field_type,
                                        ns,
                                        log,
                                        is_union);
    values.push_back(typecast_exprt::conditional_cast(value, field_type));
  }
}

// TODO: doxygen?
static exprt or_values(const exprt::operandst &values, const typet &field_type)
{
  if(values.size() == 1)
  {
    return values[0];
  }
  return multi_ary_exprt(ID_bitor, values, field_type);
}

// TODO: doxygen?
exprt compute_or_over_cells(
  const exprt &expr,
  const typet &field_type,
  const namespacet &ns,
  const messaget &log,
  const bool is_union)
{
  const typet type = ns.follow(expr.type());

  if(type.id() == ID_struct || type.id() == ID_union)
  {
    exprt::operandst values;
    for(const auto &component : to_struct_union_type(type).components())
    {
      if(component.get_is_padding())
      {
        continue;
      }
      or_elements(
        member_exprt(expr, component),
        field_type,
        ns,
        log,
        is_union,
        values);
    }
    return or_values(values, field_type);
  }
  else if(type.id() == ID_array)
  {
    const array_typet &array_type = to_array_type(type);
    if(array_type.size().is_constant())
    {
      exprt::operandst values;
      const mp_integer size = numeric_cast_v<mp_integer>(to_constant_expr(array_type.size()));
      for(mp_integer index = 0; index < size; ++index)
      {
        or_elements(
          index_exprt(expr, from_integer(index, index_type())),
          field_type,
          ns,
          log,
          is_union,
          values);
      }
      return or_values(values, field_type);
    }
    else
    {
      log.warning()
        << "Shadow memory: cannot compute or over variable-size array "
        << format(expr)
        << messaget::eom;
    }
  }
  exprt::operandst values;
  if(is_union)
  {
    or_over_bytes(
      conditional_cast_floatbv_to_unsignedbv(expr), type, field_type, values);
  }
  else
  {
    values.push_back(typecast_exprt::conditional_cast(
      conditional_cast_floatbv_to_unsignedbv(expr), field_type));
  }
  return or_values(values, field_type);
}

// TODO: doxygen?
static void max_element(
  const exprt &element,
  const typet &field_type,
  exprt &max)
{
  exprt value = typecast_exprt::conditional_cast(element, field_type);
  if (max.is_nil())
  {
    max = value;
  }
  else {
    max = if_exprt(binary_predicate_exprt(value, ID_gt, max), value, max);
  }
}

// TODO: doxygen?
static void max_over_bytes(
  const exprt &value,
  const typet &type,
  const typet &field_type,
  exprt &max)
{
  const size_t size = to_bitvector_type(type).get_width() / 8;
  max_element(value, field_type, max);
  for(size_t i = 1; i < size; ++i)
  {
    max_element(
      lshr_exprt(value, from_integer(8 * i, size_type())),
      field_type,
      max);
  }
}

// TODO: doxygen?
static void max_elements(
  exprt element,
  const typet &field_type,
  const namespacet &ns,
  const messaget &log,
  const bool is_union,
  exprt &max)
{
  element = conditional_cast_floatbv_to_unsignedbv(element);
  if(element.type().id() == ID_unsignedbv || element.type().id() == ID_signedbv)
  {
    if(is_union)
    {
      max_over_bytes(element, element.type(), field_type, max);
    }
    else
    {
      max_element(element, field_type, max);
    }
  }
  else
  {
    exprt value = compute_max_over_cells(element,
                                         field_type,
                                         ns,
                                         log,
                                         is_union);
    max_element(value, field_type, max);
  }
}

// TODO: doxygen?
exprt compute_max_over_cells(
  const exprt &expr,
  const typet &field_type,
  const namespacet &ns,
  const messaget &log,
  const bool is_union)
{
  const typet type = ns.follow(expr.type());

  if(type.id() == ID_struct || type.id() == ID_union)
  {
    exprt max = nil_exprt();
    for(const auto &component : to_struct_union_type(type).components())
    {
      if(component.get_is_padding())
      {
        continue;
      }
      max_elements(
        member_exprt(expr, component),
        field_type,
        ns,
        log,
        is_union,
        max);
    }
    return max;
  }
  else if(type.id() == ID_array)
  {
    const array_typet &array_type = to_array_type(type);
    if(array_type.size().is_constant())
    {
      exprt max = nil_exprt();
      const mp_integer size = numeric_cast_v<mp_integer>(to_constant_expr(array_type.size()));
      for(mp_integer index = 0; index < size; ++index)
      {
        max_elements(
          index_exprt(expr, from_integer(index, index_type())),
          field_type,
          ns,
          log,
          is_union,
          max);
      }
      return max;
    }
    else
    {
      log.warning()
        << "Shadow memory: cannot compute max over variable-size array "
        << format(expr)
        << messaget::eom;
    }
  }
  // TODO: This is incorrect when accessing non-0 offsets of scalars.
  return typecast_exprt::conditional_cast(
    conditional_cast_floatbv_to_unsignedbv(expr), field_type);
}
