// Author: Diffblue Ltd.

#include "pointer_size_tracking.h"

#include <util/pointer_expr.h>
#include <util/pointer_offset_size.h>

// Initialisation code
pointer_size_mapt initial_pointer_size_map()
{
  pointer_size_mapt pointer_size_map;
  return pointer_size_map;
}

void track_pointer_sizes(
  const exprt &expression,
  const namespacet &ns,
  pointer_size_mapt &pointer_size_map)
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
      auto pointer_size = size_of_expr(pointer.type(), ns);
      
      // add to map
      const auto find_result = pointer_size_map.find(expression);
      if (find_result != pointer_size_map.cend())
        return;
    
      pointer_size_map.emplace_hint(find_result, expression, pointer_size.value());
    }
}
