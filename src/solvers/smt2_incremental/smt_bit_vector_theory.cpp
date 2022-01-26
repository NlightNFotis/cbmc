// Author: Diffblue Ltd.

#include <map>

#include <solvers/smt2_incremental/smt_bit_vector_theory.h>
#include <solvers/smt2_incremental/smt_sorts.h>

#include <util/invariant.h>

std::map<std::string, smt_sortt> op_to_sort{
  // Operations that are predicates
  {"bvult", smt_bool_sortt{}},
  {"bvule", smt_bool_sortt{}},
  {"bvugt", smt_bool_sortt{}},
  {"bvuge", smt_bool_sortt{}},
  {"bvslt", smt_bool_sortt{}},
  {"bvsle", smt_bool_sortt{}},
  {"bvsgt", smt_bool_sortt{}},
  {"bvsge", smt_bool_sortt{}},
  // Arithmetic operations returning bit-vectors
  {"bvadd", smt_bit_vector_sortt{8}},
  {"bvsub", smt_bit_vector_sortt{8}},
  {"bvmul", smt_bit_vector_sortt{8}},
  {"bvudiv", smt_bit_vector_sortt{8}},
  {"bvurem", smt_bit_vector_sortt{8}},
  // Unary operations returning bit-vectors
  {"bvneg", smt_bit_vector_sortt{8}},
};

static void validate_bitvector_operator_arguments(
  const smt_termt &left,
  const smt_termt &right)
{
  const auto left_sort = left.get_sort().cast<smt_bit_vector_sortt>();
  INVARIANT(left_sort, "Left operand must have bitvector sort.");
  const auto right_sort = right.get_sort().cast<smt_bit_vector_sortt>();
  INVARIANT(right_sort, "Right operand must have bitvector sort.");
  // The below invariant is based on the smtlib standard.
  // See http://smtlib.cs.uiowa.edu/logics-all.shtml#QF_BV
  INVARIANT(
    left_sort->bit_width() == right_sort->bit_width(),
    "Left and right operands must have the same bit width.");
}

#define SMT_BITVECTOR_THEORY_OPERATOR(the_identifier, the_name)                \
  void smt_bit_vector_theoryt::the_name##t::validate(                          \
    const smt_termt &left, const smt_termt &right)                             \
  {                                                                            \
    validate_bitvector_operator_arguments(left, right);                        \
  }                                                                            \
                                                                               \
  smt_sortt smt_bit_vector_theoryt::the_name##t::return_sort(                  \
    const smt_termt &, const smt_termt &)                                      \
  {                                                                            \
    return op_to_sort[#the_identifier];                                        \
  }                                                                            \
                                                                               \
  const char *smt_bit_vector_theoryt::the_name##t::identifier()                \
  {                                                                            \
    return #the_identifier;                                                    \
  }                                                                            \
                                                                               \
  const smt_function_application_termt::factoryt<                              \
    smt_bit_vector_theoryt::the_name##t>                                       \
    smt_bit_vector_theoryt::the_name{};
#include "smt_bit_vector_theory.def"
#undef SMT_BITVECTOR_THEORY_OPERATOR

// static void validate_bit_vector_arith_op_arguments(
//   const smt_termt &left,
//   const smt_termt &right)
// {
//   // The requirement for arithmetic operations is that both
//   // operands have the same sort
//   const auto left_sort = left.get_sort().cast<smt_bit_vector_sortt>();
//   INVARIANT(left_sort, "Left operand must have bitvector sort.");
//   const auto right_sort = right.get_sort().cast<smt_bit_vector_sortt>();
//   INVARIANT(right_sort, "Right operand must have bitvector sort.");
// }

