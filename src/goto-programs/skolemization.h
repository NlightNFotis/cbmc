/// \file skolemization.h
/// Rename variables in existentially quantified statements.

#ifndef CPROVER_GOTO_PROGRAMS_SKOLEMIZATION_H
#define CPROVER_GOTO_PROGRAMS_SKOLEMIZATION_H

class exprt;
class goto_modelt;

/// Rename bound variables in existential quantifiers. This is the entry
/// point to the module.
/// \param goto_model The goto_model on which we operate. Needed
///    for making changes to the symbol table at variable renaming time.
void rename_variables_in_existential_quantifiers(goto_modelt &goto_model);

/// Helper function that recurses over the condition expression of instructions.
/// \param cond The condition on which we recurse over the operands (subexpressions).
void recurse_on_condition(exprt &cond, goto_modelt& model);

/// Helper function that does the actual renaming of the variables.
/// This is called when we reach the atomic parts of the condition expression,
/// to identify and rename the variables of any quantified statements that
/// exist as part of the guard.
/// \param cond The condition in which we are doing the renaming.
/// \param model The 
void rename_and_substitute(exprt &cond, goto_modelt& model);

#endif
