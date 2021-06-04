/// \file
/// \author Diffblue Ltd.
/// Unit tests for checking the piped process communication mechanism.

#ifdef _WIN32
// No unit tests yet!
#else

#  include <testing-utils/use_catch.h>
#  include <util/piped_process.h>

TEST_CASE(
  "We create a pipe and we can read from it",
  "[core][util][piped_process]")
{
  const std::string to_be_echoed = "The Jabberwocky";
  // Need to give path to avoid shell built-in invocation
  const std::string binary = "/bin/echo";
  const std::string command = binary + " " + to_be_echoed;
  piped_processt process = piped_processt(command);

  // This is an indirect way to detect when the pipe has something since 
  // -1 is an infinite timeout wait. This could (in theory) also return
  // when there is an error, but this unit test is not doing error handling.
  process.can_receive(-1);
  std::string response = process.receive();

  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());

  REQUIRE(response == to_be_echoed);
}

TEST_CASE(
  "We create a pipe, send and receive from it",
  "[core][util][piped_process]")
{
  const std::string binary = "z3 -in";
  const std::string statement = "(echo \"hi\")\n";
  const std::string termination_statement = "(exit)\n";
  piped_processt process = piped_processt(binary);

  piped_processt::process_send_responset res = process.send(statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.can_receive(-1);
  std::string response = process.receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("hi"));
  REQUIRE(response.length() == 2);

  res = process.send(termination_statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);
}

TEST_CASE("We create a pipe, interact", "[core][util][piped_process]")
{
  const std::string binary = "z3 -in";
  std::string statement = "(echo \"hi\")\n";
  const std::string termination_statement = "(exit)\n";
  piped_processt process = piped_processt(binary);

  piped_processt::process_send_responset res = process.send(statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.can_receive(-1);
  std::string response = process.receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("hi"));

  statement = std::string("(echo \"Second string\")\n");
  res = process.send(statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.can_receive(-1);
  response = process.receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("Second string"));

  res = process.send(termination_statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);
}

TEST_CASE("Use pipe to solve a simple SMT problem", "[core][util][piped_process]")
{
  const std::string binary = "z3 -in -smt2";
  const std::string termination_statement = "(exit)\n";
  piped_processt process = piped_processt(binary);

  piped_processt::process_send_responset res = process.send("(set-logic QF_LIA) (declare-const x Int) (declare-const y Int) (assert (> (+ (mod x 4) (* 3 (div y 2))) (- x y)))  (check-sat)\n");
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.can_receive(-1);
  std::string response = process.receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("sat"));

  res = process.send(termination_statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);
}

TEST_CASE("Use pipe to solve a simple SMT problem and get the model", "[core][util][piped_process]")
{
  const std::string binary = "z3 -in -smt2";
  const std::string termination_statement = "(exit)\n";
  piped_processt process = piped_processt(binary);

  piped_processt::process_send_responset res = process.send("(set-logic QF_LIA) (declare-const x Int) (declare-const y Int) (assert (> (+ (mod x 4) (* 3 (div y 2))) (- x y)))  (check-sat)\n");
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.can_receive(-1);
  std::string response = process.receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("sat"));

  res = process.send("(get-model)\n");
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.can_receive(-1);
  response = process.receive();
  // Since the above two lines will read IMMEDIATELY when data is available on
  // the pipe, it is likely that only the first line will be available.
  // Hence the check below is for the first line of the response.
  // Note that checking the whole model is done in another case below, and
  // the goal of the piped_process code is to enable interaction, parsing of
  // complex responses is left to the caller.
  REQUIRE(response.substr(0,6) == std::string("(model"));

  res = process.send(termination_statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);
}


TEST_CASE("Use pipe to solve a simple SMT problem with wait_receive", "[core][util][piped_process]")
{
  const std::string binary = "z3 -in -smt2";
  const std::string termination_statement = "(exit)\n";
  piped_processt process = piped_processt(binary);

  piped_processt::process_send_responset res = process.send("(set-logic QF_LIA) (declare-const x Int) (declare-const y Int) (assert (> (+ (mod x 4) (* 3 (div y 2))) (- x y)))  (check-sat)\n");
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  std::string response = process.wait_receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("sat"));

  res = process.send(termination_statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);
}

TEST_CASE("We create a pipe, interact, use wait_receivable", "[core][util][piped_process]")
{
  const std::string binary = "z3 -in";
  std::string statement = "(echo \"hi\")\n";
  const std::string termination_statement = "(exit)\n";
  piped_processt process = piped_processt(binary);

  piped_processt::process_send_responset res = process.send(statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.wait_receivable(100);
  std::string response = process.receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("hi"));

  statement = std::string("(echo \"Second string\")\n");
  res = process.send(statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.wait_receivable(100);
  response = process.receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("Second string"));

  res = process.send(termination_statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);
}


TEST_CASE("Use pipe to solve a simple SMT problem and get the model, with wait_receivable/can_receive", "[core][util][piped_process]")
{
  const std::string binary = "z3 -in -smt2";
  const std::string termination_statement = "(exit)\n";
  piped_processt process = piped_processt(binary);

  piped_processt::process_send_responset res = process.send("(set-logic QF_LIA) (declare-const x Int) (declare-const y Int) (assert (> (+ (mod x 4) (* 3 (div y 2))) (- x y)))  (check-sat)\n");
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.wait_receivable(100);
  std::string response = process.receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("sat"));

  res = process.send("(get-model)\n");
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.wait_receivable(500);
  response = process.receive();
  REQUIRE(response == std::string("(model \n  (define-fun y () Int\n    0)\n  (define-fun x () Int\n    (- 4))\n)\n"));

  res = process.send(termination_statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);
}

TEST_CASE("Use pipe to solve a simple SMT problem and get the model, can_receive(-1)", "[core][util][piped_process]")
{
  const std::string binary = "z3 -in -smt2";
  const std::string termination_statement = "(exit)\n";
  piped_processt process = piped_processt(binary);

  piped_processt::process_send_responset res = process.send("(set-logic QF_LIA) (declare-const x Int) (declare-const y Int) (assert (> (+ (mod x 4) (* 3 (div y 2))) (- x y)))  (check-sat)\n");
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);

  process.can_receive(-1);
  std::string response = process.receive();
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("sat"));

  res = process.send(termination_statement);
  REQUIRE(res == piped_processt::process_send_responset::SUCCEEDED);
}

#endif
