/// \file Subprocess communication with pipes
///
/// \author Diffblue Ltd.


#ifdef _WIN32
// Windows includes go here
#else
#  include <fcntl.h>  // library for fcntl function
#  include <poll.h>   // library for poll function
#  include <signal.h> // library for kill function
#  include <unistd.h> // library for read/write/sleep/etc. functions
#endif

#include "invariant.h"
#include "piped_process.h"
#include "string_utils.h"

#define BUFSIZE 2048

piped_processt::piped_processt(const std::string &command)
{
#ifdef _WIN32
  UNIMPLEMENTED_FEATURE("Pipe IPC on windows.")
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
  pid = fork();
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
    char **args = string_to_cstr_array(command, " ");

    // Execute the command
    execvp(args[0], args);
    // The args variable will be handled by the OS if execvp succeeds, but
    // if execvp fails then we should free it here (just in case the runtime
    // error below continues execution.)
    free(args);
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

piped_processt::~piped_processt()
{
#ifdef _WIN32
  UNIMPLEMENTED_FEATURE("Pipe IPC on windows")
#else
  // Close the parent side of the remaning pipes
  fclose(command_stream);
  // Note that the above will call close(pipe_input[1]);
  close(pipe_output[0]);
  // Send signal to the child process to terminate
  kill(pid, SIGTERM);
#endif
}

piped_processt::process_send_responset piped_processt::send(const std::string &message)
{
#ifdef _WIN32
  UNIMPLEMENTED_FEATURE("Pipe IPC on windows")
#else

  if(process_state != process_statet::CREATED)
  {
    return process_send_responset::ERROR;
  }

  // send message to solver process
  int send_status = fputs(message.c_str(), command_stream);
  fflush(command_stream);

  if(send_status == EOF)
  {
    // Some kind of error occured, maybe we should update the
    // process status here?
    return process_send_responset::FAILED;
  }

  return process_send_responset::SUCCEEDED;
#endif
}

std::string piped_processt::receive()
{
#ifdef _WIN32
  UNIMPLEMENTED_FEATURE("Pipe IPC on windows: receive()")
#else

  INVARIANT(process_state == process_statet::CREATED,
    "Can only receive() from a fully initialised process");
  
  // This is necessary to ensure the buffer is synced with the
  // latest data.
  fsync(pipe_output[0]);

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
      return response;
    default:
      // Read some bytes, append them to the response and continue
      response.append(buff, nbytes);
    }
  }

  UNREACHABLE;
#endif
}

std::string piped_processt::wait_receive()
{
  // can_receive(-1) waits an ubounded time until there is some data
  can_receive(-1);
  return receive();
}

piped_processt::process_statet piped_processt::get_status()
{
  return process_state;
}

bool piped_processt::can_receive(int timeout)
{
#ifdef _WIN32
  UNIMPLEMENTED_FEATURE("Pipe IPC on windows: can_receive(int timeout)")
#else
  int ready;
  struct pollfd fds { pipe_output[0], POLLIN, 0 };
  nfds_t nfds = POLLIN;
  ready = poll(&fds, nfds, timeout);
  switch (ready)
  {
  case -1:
    // Error case
    // Further error handling could go here
    process_state = process_statet::ERROR;
    // fallthrough intended
  case 0:
    // Timeout case
    // Do nothing for timeout and error fallthrough, default is return false.
    break;
  default:
    // Found some events, check for POLLIN
    if(fds.revents & POLLIN)
    {
      // we can read from the pipe here
      return true;
    }
    // Some revent we did not ask for or check for, can't read though.
  }
  return false;
#endif  
}

bool piped_processt::can_receive()
{
  return can_receive(0);
}

void piped_processt::wait_receivable(int wait_time)
{
#ifdef _WIN32
  UNIMPLEMENTED_FEATURE("Pipe IPC on windows: wait_stopped(int wait_time)")
#else
  while(process_state == process_statet::CREATED && !can_receive(0))
  {
    usleep(wait_time);
  }
#endif
}
