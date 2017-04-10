/*******************************************************************\

 Module: analyses variable-sensitivity

 Author: Fotis Koutoulakis, fotis.koutoulakis@diffblue.com

\*******************************************************************/

#include <util/std_expr.h>
#include <util/namespace.h>
#include <analyses/variable-sensitivity/abstract_enviroment.h>

#include "union_abstract_object.h"


/* First constructor */
union_abstract_objectt::union_abstract_objectt(const typet &type):
  abstract_objectt(type)
{
  assert(type.id()==ID_union);
}

/* Second constructor */
union_abstract_objectt::union_abstract_objectt(
  const typet &type, bool top, bool bottom):
    abstract_objectt(type, top, bottom)
{
  assert(type.id()==ID_union);
}

/* Third constructor */
union_abstract_objectt::union_abstract_objectt(
  const exprt &expr, const abstract_environmentt &environment,
  const namespacet &ns):
    abstract_objectt(expr, environment, ns)
{
  assert(ns.follow(expr.type()).id()==ID_union);
}


/* Copy constructor */
union_abstract_objectt::union_abstract_objectt(
  const union_abstract_objectt &old):
    abstract_objectt(old)
{}

/* read component */
abstract_object_pointert union_abstract_objectt::read_component(
  const abstract_environmentt &environment,
  const member_exprt &member_expr,
  const namespacet &ns) const
{
  return environment.abstract_object_factory(
    member_expr.type(), ns, !is_bottom(), is_bottom());
}

/* write component */
sharing_ptrt<union_abstract_objectt> union_abstract_objectt::write_component(
  abstract_environmentt &environment,
  const namespacet &ns,
  const std::stack<exprt> &stack,
  const member_exprt &member_expr,
  const abstract_object_pointert value,
  bool merging_write) const
{
  if(is_top() || is_bottom())
  {
    return sharing_ptrt<union_abstract_objectt>(
      dynamic_cast<union_abstract_objectt*>(clone()));
  }
  else
  {
    return sharing_ptrt<union_abstract_objectt>(
      new union_abstract_objectt(type(), true, false));
  }
}
