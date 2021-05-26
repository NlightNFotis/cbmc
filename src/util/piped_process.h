#ifndef CPROVER_UTIL_PIPED_PROCESS_H
#define CPROVER_UTIL_PIPED_PROCESS_H

// TODO: Revisit buffer size and storage location.
// Note that this may impact number of characters correctly communicated
// by the pipe, to check/fix.
#define BUFSIZE 2048

class piped_processt
{
  enum class process_statet
  {
    NOT_CREATED,
    CREATED,
    STOPPED
  };

  char **split_command_args(const std::string &command);

protected:
  FILE *response_stream;
  FILE *command_stream;
  int pipe_input[2];
  int pipe_output[2];
  process_statet process_state;

public:
  bool send(const std::string &message);
  std::string receive();

  process_statet get_status()
  {
    return process_state;
  }

  piped_processt(const std::string &command);
};

#endif
