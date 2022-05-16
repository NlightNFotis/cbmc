// Author: Diffblue Ltd.

#include "pointer_size_tracking.h"

#include <util/invariant.h>
#include <util/pointer_expr.h>
#include <util/pointer_offset_size.h>

#include <solvers/smt2_incremental/convert_expr_to_smt.h>

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
    if(
    std::any_of(
      expression.operands().cbegin(), expression.operands().cend(), [](exprt operand) {
    return can_cast_type<pointer_typet>(operand.type());
    }))
    {
      exprt pointer;
      // identify pointer expression
      for(auto &operand : expression.operands())
      {
        if(can_cast_type<pointer_typet>(operand.type()))
        {
          pointer = operand;
        }
      }

      // get size of pointer
      auto pointer_size_expr = size_of_expr(pointer.type(), ns);
      PRECONDITION(pointer_size_expr.has_value());
      auto pointer_size_term = convert_expr_to_smt(pointer_size_expr.value(), object_map, object_size);
      
      // add to map
      const auto find_result = pointer_size_map.find(expression);
      if (find_result != pointer_size_map.cend())
        return;
    
      pointer_size_map.emplace_hint(find_result, expression, pointer_size_term);
    }
}
