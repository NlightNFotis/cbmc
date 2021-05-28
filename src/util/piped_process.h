 /// \file Subprocess communication with pipes
///
/// \author Diffblue Ltd.

#ifndef CPROVER_UTIL_PIPED_PROCESS_H
#define CPROVER_UTIL_PIPED_PROCESS_H

class piped_processt
{
  char **split_command_args(const std::string &command);

public:
  enum class process_statet
  {
    NOT_CREATED,
    CREATED,
    STOPPED
  };
  
  bool send(const std::string &message);
  std::string receive();

  process_statet get_status();

  piped_processt(const std::string &command);

protected:
  FILE *command_stream;
  int pipe_input[2];
  int pipe_output[2];
  process_statet process_state;

};

#endif
