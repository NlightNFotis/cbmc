/// \file Subprocess communication with pipes
///
/// \author Diffblue Ltd.

#ifndef CPROVER_UTIL_PIPED_PROCESS_H
#define CPROVER_UTIL_PIPED_PROCESS_H

class piped_processt
{
public:
  /// Enumeration to keep track of child process state.
  enum class process_statet
  {
    NOT_CREATED,
    CREATED,
    STOPPED,
    ERROR
  };

  enum class process_send_responset
  {
    SUCCEEDED,
    FAILED,
    ERROR
  };
  
  /// Send a string message (command) to the child process.
  /// \param message The string message to be sent.
  process_send_responset send(const std::string &message);
  /// Read a string from the child process' output.
  std::string receive();
  /// Wait until a string is available and read a string from the child
  /// process' output.
  std::string wait_receive();

  /// Get child process status.
  process_statet get_status();

  /// See if this process can receive data from the other process.
  /// \param timeout Amount of time to wait before timing out and
  ///     returning: -1 is infiniute, 0 is non-blocking immediate.
  bool can_receive(int timeout);

  /// See if this process can receive data from the other process.
  /// Note this calls can_receive(0);
  bool can_receive();

  /// Wait for the pipe to be ready, waiting specified time between
  /// checks. Will return when the pipe is ready or the other process
  /// is not in a process_statet::CREATED state (i.e. error has occured).
  /// \param wait_time Time spent in usleep() between checks of can_receive(0)
  void wait_receivable(int wait_time);

  /// Initiate a new subprocess with pipes supporting communication
  /// between the parent (this process) and the child.
  /// \param command The command we want to run (binary + options)
  piped_processt(const std::string &command);

  ~piped_processt();

protected:
  // Child process ID.
  pid_t pid;
  FILE *command_stream;
  int pipe_input[2];
  int pipe_output[2];
  process_statet process_state;
};

#endif
