#include <cstring>
#include <iostream>
#include <unistd.h>
#include <errno.h>

#include "piped_process.h"


piped_processt::piped_processt(const std::string &command)
{
    // piped_processt subp = piped_processt();
    if (pipe(pipe_input) == -1) {
        throw std::runtime_error("Input pipe creation failed");
    }

    if (pipe(pipe_output) == -1) {
        throw std::runtime_error("Output pipe creation failed");
    }
    // Default state
    process_state = process_statet::NOT_CREATED;
    
    // Create a new process for the child that will execute the
    // command and receive information via pipes.
    pid_t pid = fork();
    if (pid == 0) {
        // child process here

        // Close pipes that will be used by the parent so we do
        // not have our own copies and conflicts.
        close(pipe_input[1]);
        close(pipe_output[0]);

        // Duplicate pipes so we have the ones we need.
        dup2(pipe_input[0],  STDIN_FILENO);
        dup2(pipe_output[1], STDOUT_FILENO);
        dup2(pipe_output[1], STDERR_FILENO);

        // Create the arguments to execvp from the construction string
        char **args = split_command_args(command);
        // Execute the command
        execvp(args[0], args);
        // Only reachable if execpv failed
        throw std::runtime_error("Launching \"" + command +
                "\" failed with error: "  + strerror(errno));
    } else {
        // parent process here
        // Close pipes to be used by the child process
        close(pipe_input[0]);
        close(pipe_output[1]);

        // get stream for sending to the child process
        command_stream = fdopen(pipe_input[1], "w");
        process_state = process_statet::CREATED;
    }
}

bool piped_processt::send(const std::string &message)
{
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
}

std::string piped_processt::receive()
{
    if(process_state != process_statet::CREATED)
        return NULL;
    std::string response = std::string("");
    int nbytes;
    char buff[BUFSIZE];
    while (true)
    {
        nbytes = read(pipe_output[0], buff, BUFSIZE);
        if (nbytes == 0)
            break;
        response.append(buff);
    }
    // assume nothing can go wrong for now! :D
    return response;
}


char ** piped_processt::split_command_args(const std::string &command)
{
    char ** res  = NULL;
    int n_spaces = 0;
    char *p = strtok(strdup(command.c_str()), " ");
    while(p)
    {
          res = (char **)realloc(res, sizeof (char*) * ++n_spaces);
          if (res == NULL)
            exit (-1); /* memory allocation failed */
          res[n_spaces-1] = p;
          p = strtok (NULL, " ");
    }
    res = (char **)realloc (res, sizeof (char*) * (n_spaces+1));
    res[n_spaces] = 0;
    return res;
}

// Below is simple testing code to see that things (mostly) work.
// int main(int argc, char *argv[])
// {
//     piped_processt subp = piped_processt("/usr/local/bin/z3 --help");
//     std::string data = subp.receive();
//     std::cout << data << std::endl;
//     return 0;
// }
