/// \file skolemization.h
/// Rename variables in existentially quantified statements.

#include "skolemization.h"

#include "goto_model.h"

#include <ansi-c/expr2c.h>

#include <util/expr_util.h>
#include <util/fresh_symbol.h>
#include <util/irep.h>                // for id2string
#include <util/mathematical_expr.h>

void rename_variables_in_existential_quantifiers(goto_modelt &goto_model) {
  for (auto &function : goto_model.goto_functions.function_map) {
    for (auto &instruction : function.second.body.instructions) {
      if (instruction.has_condition()) {
        auto &cond = instruction.condition_nonconst();
        recurse_on_condition(cond, goto_model);
      }
    }
  }
}

void recurse_on_condition(exprt &cond, goto_modelt& model) {
  // recurse on the condition's subexpressions first - that allows us
  // to correctly identify the nodes we care about in a compound
  // expression, for instance something like `1 == 1 && exists(i.i>=0 && i>1)`
  for (exprt &subexpr : cond.operands()) {
    recurse_on_condition(subexpr, model);
  }

  // if the expression we're in doesn't have any more operands,
  // it means it's a leaf node, so we go ahead and process it.
  rename_and_substitute(cond, model);
}

void rename_and_substitute(exprt &cond, goto_modelt& model) {
  // perform the first part of skolemisation, by identifying an existentially
  // quantified statement and substituting the variables with skolem constants.
  if (can_cast_expr<exists_exprt>(cond)) {
    quantifier_exprt &quantifier_expr = to_quantifier_expr(cond);
    const auto &quantifier_sym = quantifier_expr.symbol();

    const auto new_symbol = get_fresh_aux_symbol(
        quantifier_sym.type(),
        CPROVER_PREFIX,
        id2string(model.symbol_table.lookup(quantifier_sym.get_identifier())->base_name),
        quantifier_expr.source_location(),
        ID_C,
        model.symbol_table);
    const auto symbol_expr = symbol_exprt(new_symbol.name, quantifier_sym.type());
    const std::vector<exprt> new_sym_vec{symbol_expr};
    
    cond = exists_exprt(symbol_expr, quantifier_expr.instantiate(new_sym_vec));
  }
}
