// Author: Diffblue Ltd.

#include "pointer_size_tracking.h"

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
      auto pointer_type = to_pointer_type(pointer.type());
      auto pointer_size_expr = size_of_expr(pointer_type, ns);
      PRECONDITION(pointer_size_expr.has_value());
      auto pointer_size_term = convert_expr_to_smt(pointer_size_expr.value(), object_map, pointer_size_map, object_size);

      std::cout << "[DEBUG] pointer_size_term is " << pointer_size_term.pretty() << std::endl; 
      
      // add to map
      const auto find_result = pointer_size_map.find(pointer_type.base_type());
      if (find_result != pointer_size_map.cend())
        return;
    
      std::cout << "[DEBUG] adding to map: " << pointer_type.base_type().pretty() << std::endl;
      pointer_size_map.emplace_hint(find_result, pointer_type.base_type(), pointer_size_term);
    }
}
