/// \file Subprocess communication with pipes
///
/// \author Diffblue Ltd.

#ifdef _WIN32
// Windows includes go here
#else
#  include <cstring>
#  include <errno.h>
#  include <fcntl.h> // library for fcntl function
#  include <iostream>
#  include <unistd.h>
#endif

#include "invariant.h"
#include "piped_process.h"

piped_processt::piped_processt(const std::string &command)
{
#ifdef _WIN32
  // This should use the new error state from PR #6131 once that is done
  INVARIANT(false, "New SMT2 backend WIP: Windows piped_process constructor.");
#else

  if(pipe(pipe_input) == -1)
  {
    throw std::runtime_error("Input pipe creation failed");
  }

  if(pipe(pipe_output) == -1)
  {
    throw std::runtime_error("Output pipe creation failed");
  }

  // Default state
  process_state = process_statet::NOT_CREATED;

  if(fcntl(pipe_output[0], F_SETFL, O_NONBLOCK) < 0)
  {
    throw std::runtime_error("Setting pipe non-blocking failed");
  }

  // Create a new process for the child that will execute the
  // command and receive information via pipes.
  pid_t pid = fork();
  if(pid == 0)
  {
    // child process here

    // Close pipes that will be used by the parent so we do
    // not have our own copies and conflicts.
    close(pipe_input[1]);
    close(pipe_output[0]);

    // Duplicate pipes so we have the ones we need.
    dup2(pipe_input[0], STDIN_FILENO);
    dup2(pipe_output[1], STDOUT_FILENO);
    dup2(pipe_output[1], STDERR_FILENO);

    // Create the arguments to execvp from the construction string
    char **args = split_command_args(command);
    // Execute the command
    execvp(args[0], args);
    // Only reachable if execvp failed
    throw std::runtime_error(
      "Launching \"" + command + "\" failed with error: " + strerror(errno));
  }
  else
  {
    // parent process here
    // Close pipes to be used by the child process
    close(pipe_input[0]);
    close(pipe_output[1]);

    // Get stream for sending to the child process
    command_stream = fdopen(pipe_input[1], "w");
    process_state = process_statet::CREATED;
  }
#endif
}

bool piped_processt::send(const std::string &message)
{
#ifdef _WIN32
  // This should use the new error state from PR #6131 once that is done
  INVARIANT(false, "New SMT2 backend WIP: Windows piped_processt::send.");
#else

  if(process_state != process_statet::CREATED)
  {
    return false;
  }

  // send message to solver process
  int send_status = fputs(message.c_str(), command_stream);
  fflush(command_stream);

  if(send_status == EOF)
  {
    // Some kind of error occured, maybe we should update the
    // solver status here?
    return false;
  }

  return true;
#endif
}

std::string piped_processt::receive()
{
#ifdef _WIN32
  // This should use the new error state from PR #6131 once that is done
  INVARIANT(false, "New SMT2 backend WIP: Windows piped_processt::receive.");
#else

  if(process_state != process_statet::CREATED)
    return NULL;

  std::string response = std::string("");
  int nbytes;
  char buff[BUFSIZE];

  while(true)
  {
    nbytes = read(pipe_output[0], buff, BUFSIZE);
    switch(nbytes)
    {
    case -1:
      // Nothing more to read in the pipe
      return response;
    case 0:
      // Pipe is closed.
      process_state = process_statet::STOPPED;
      if(response == std::string(""))
      {
        return NULL;
      }
      return response;
    default:
      // Read some bytes, append them to the response and continue
      response.append(buff, nbytes);
    }
  }

  UNREACHABLE;
#endif
}

char **piped_processt::split_command_args(const std::string &command)
{
  char **res = NULL;
  int n_spaces = 0;
  char *p = strtok(strdup(command.c_str()), " ");

  while(p)
  {
    res = reinterpret_cast<char **>(realloc(res, sizeof(char *) * ++n_spaces));
    if(res == NULL)
      exit(-1); /* memory allocation failed */
    res[n_spaces - 1] = p;
    p = strtok(NULL, " ");
  }

  res =
    reinterpret_cast<char **>(realloc(res, sizeof(char *) * (n_spaces + 1)));
  res[n_spaces] = 0;
  return res;
}
