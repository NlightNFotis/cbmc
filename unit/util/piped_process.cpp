/// \file
/// \author Diffblue Ltd.
/// Unit tests for checking the piped process communication mechanism.

#ifdef _WIN32
// No unit tests yet!
#else

#  include <testing-utils/use_catch.h>
#  include <unistd.h>
#  include <util/piped_process.h>

TEST_CASE(
  "We create a pipe and we can read from it",
  "[core][util][piped_process]")
{
  std::string to_be_echoed = "The Jabberwocky";
  // Need to give up to avoid shell built-in invocation
  std::string binary = "/bin/echo";
  std::string command = binary + " " + to_be_echoed;
  piped_processt process = piped_processt(command);

  sleep(1);

  std::string response = process.receive();

  // Trim newline from response string that causes match to fail.
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());

  REQUIRE(response == to_be_echoed);
}

TEST_CASE(
  "We create a pipe, send and receive from it",
  "[core][util][piped_process]")
{
  std::string binary = "z3 -in";
  std::string statement = "(echo \"hi\")";
  std::string termination_statement = "(exit)";
  piped_processt process = piped_processt(binary);

  bool res = process.send(statement);
  REQUIRE(res == true); // sending succeeded without problems

  // Wait a moment for z3 to process the sent message
  sleep(1);
  std::string response = process.receive();
  // Trim newline from response string that causes match to fail.
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("hi"));
  REQUIRE(response.length() == 2);

  // Tell z3 to terminate
  res = process.send(termination_statement);
  REQUIRE(res == true);
}

TEST_CASE("We create a pipe, interact", "[core][util][piped_process]")
{
  std::string binary = "z3 -in";
  std::string statement = "(echo \"hi\")";
  std::string termination_statement = "(exit)";
  piped_processt process = piped_processt(binary);

  bool res = process.send(statement);
  REQUIRE(res == true); // sending succeeded without problems

  // Wait a moment for z3 to process the sent message
  sleep(1);
  std::string response = process.receive();
  // Trim newline from response string that causes match to fail.
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("hi"));

  statement = std::string("(echo \"Second string\")");
  res = process.send(statement);
  REQUIRE(res == true); // sending succeeded without problems

  // Wait a moment for z3 to process the sent message
  sleep(1);
  response = process.receive();
  // Trim newline from response string that causes match to fail.
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("Second string"));

  // Tell z3 to terminate
  res = process.send(termination_statement);
  REQUIRE(res == true);
}

TEST_CASE("Use pipe to solve a simple SMT problem", "[core][util][piped_process]")
{
  std::string binary = "z3 -in -smt2";
  std::string termination_statement = "(exit)";
  piped_processt process = piped_processt(binary);

  bool res = process.send("(set-logic QF_LIA) (declare-const x Int) (declare-const y Int) (assert (> (+ (mod x 4) (* 3 (div y 2))) (- x y)))  (check-sat) ");
  REQUIRE(res == true); // sending succeeded without problems

  // Wait a moment for z3 to process the sent message
  sleep(1);
  std::string response = process.receive();
  // Trim newline from response string that causes match to fail.
  response.erase(
    std::remove(response.begin(), response.end(), '\n'), response.end());
  REQUIRE(response == std::string("sat"));

  // Tell z3 to terminate
  res = process.send(termination_statement);
  REQUIRE(res == true);
}

// TEST_CASE("Use pipe to solve a simple SMT problem and get the model", "[core][util][piped_process]")
// {
//   std::string binary = "z3 -in -smt2";
//   std::string request_model = "(get-model)";
//   std::string termination_statement = "(exit)";
//   piped_processt process = piped_processt(binary);
//   // Wait a moment for z3 to start
//   sleep(1);

//   bool res = process.send("(set-logic QF_LIA) (declare-const x Int) (declare-const y Int) (assert (> (+ (mod x 4) (* 3 (div y 2))) (- x y)))  (check-sat) ");
//   REQUIRE(res == true); // sending succeeded without problems

//   // sleep(1);
//   // std::string response = process.receive();
//   // Trim newline from response string that causes match to fail.
//   // response.erase(
//     // std::remove(response.begin(), response.end(), '\n'), response.end());
//   // REQUIRE(response == std::string(""));


//   // res = process.send("(assert (= (bvadd x (_ bv3 16)) (_ bv17 16)))");
//   // REQUIRE(res == true); // sending succeeded without problems

//   // sleep(1);
//   // response = process.receive();
//   // Trim newline from response string that causes match to fail.
//   // response.erase(
//     // std::remove(response.begin(), response.end(), '\n'), response.end());
//   // REQUIRE(response == std::string(""));


// //   res = process.send("(check-sat)");
//   // res = process.send("(echo \"sat\")");
//   // REQUIRE(res == true); // sending succeeded without problems

//   // Wait a moment for z3 to process the sent message
//   sleep(1);
//   std::string response = process.receive();
//   // Trim newline from response string that causes match to fail.
//   response.erase(
//     std::remove(response.begin(), response.end(), '\n'), response.end());
//   REQUIRE(response == std::string("sat"));

//   res = process.send(request_model);
//   REQUIRE(res == true); // sending succeeded without problems

//   // Wait a moment for z3 to process the sent message
//   sleep(1);
//   response = process.receive();
//   // Trim newline from response string that causes match to fail.
//   // response.erase(
//   //   std::remove(response.begin(), response.end(), '\n'), response.end());
//   REQUIRE(response == std::string("(get-model)\n(model\n  (define-fun y () Int\n    10)\n  (define-fun x () Int\n    20)\n)"));

//   // Tell z3 to terminate
//   res = process.send(termination_statement);
//   REQUIRE(res == true);
// }

#endif
