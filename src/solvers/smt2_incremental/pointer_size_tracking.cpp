// Author: Diffblue Ltd.

#include "pointer_size_tracking.h"

#include <util/arith_tools.h>
#include <util/c_types.h>
#include <util/invariant.h>
#include <util/pointer_expr.h>
#include <util/pointer_offset_size.h>

#include <solvers/smt2_incremental/convert_expr_to_smt.h>

#include <iostream>

// Initialisation code
pointer_size_mapt initial_pointer_size_map()
{
  pointer_size_mapt pointer_size_map;
  return pointer_size_map;
}

void associate_pointer_sizes(
  const exprt &expression,
  const namespacet &ns,
  pointer_size_mapt &pointer_size_map,
  const smt_object_mapt &object_map,
  const smt_object_sizet::make_applicationt &object_size)
{
    if(std::any_of(
        expression.operands().cbegin(),
        expression.operands().cend(),
        [](exprt operand) {
          return can_cast_type<pointer_typet>(operand.type());
    }))
    {
      exprt pointer;
      for(auto &operand : expression.operands())
      {
        if(can_cast_type<pointer_typet>(operand.type()))
        {
          pointer = operand;
        }
      }

      auto pointer_type = to_pointer_type(pointer.type());
      auto pointer_base_type = pointer_type.base_type();
      exprt pointer_size_expr;
      // There's a special case for a pointer subtype here: the case where the pointer is `void *`. This means
      // that we don't know the underlying base type, so we're just assigning a size expression value of 1 (given
      // that this is going to be used in a multiplication and 1 is the identity value for multiplcation) 
      if (is_void_pointer(pointer_type)) {
        pointer_size_expr = from_integer(1, size_type());
      } else {
        auto pointer_size_opt = size_of_expr(pointer_base_type, ns);
        PRECONDITION(pointer_size_opt.has_value());
        pointer_size_expr = pointer_size_opt.value();
      }
      auto pointer_size_term = convert_expr_to_smt(pointer_size_expr, object_map, pointer_size_map, object_size);

      const auto find_result = pointer_size_map.find(pointer_base_type);
      if (find_result != pointer_size_map.cend())
        return;
      pointer_size_map.emplace_hint(find_result, pointer_base_type, pointer_size_term);
    }
}
