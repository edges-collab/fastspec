#ifndef _SPAWN_H_
#define _SPAWN_H_

// 
// Example of communication with a subprocess via stdin/stdout
// Author: Konstantin Tretyakov
// License: MIT
//

#include <ext/stdio_filebuf.h> // NB: Specific to libstdc++
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <exception>


// ---------------------------------------------------------------------------
//
// cpipe
//
// Wrapping pipe in a class makes sure they are closed when we leave scope
//
// ---------------------------------------------------------------------------

class cpipe {

  private:

    int fd[2];
  
  public:

    // Constructor
    cpipe() { if (pipe(fd)) throw std::runtime_error("Failed to create pipe"); }

    // Destructor
    ~cpipe() { close(); }

    void close() { ::close(fd[0]); ::close(fd[1]); }

    const inline int read_fd() const { return fd[0]; }

    const inline int write_fd() const { return fd[1]; }    
};






// ---------------------------------------------------------------------------
//
// Spawn
//
// Fork the current process and execute a system call on the child.  Connect
// the child process's stdin and stdout to pipes that the parent can use.
//
// Usage:
//   spawn s(argv)
//   s.stdin << ...
//   s.stdout >> ...
//   s.send_eol()
//   s.wait()
//
// ---------------------------------------------------------------------------

class spawn {

  private:
    cpipe write_pipe;
    cpipe read_pipe;

  public:
    int child_pid = -1;
    int result;
    std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > write_buf = NULL; 
    std::unique_ptr<__gnu_cxx::stdio_filebuf<char> > read_buf = NULL;
    std::ostream stdin;
    std::istream stdout;
    
    // Constructor
    spawn( const char* const argv[], bool with_path = false, 
           const char* const envp[] = 0) : stdin(NULL), stdout(NULL) {

      child_pid = fork();

      // Error condition
      if (child_pid == -1) throw std::runtime_error("Failed to start child process"); 

      // In child process
      if (child_pid == 0) {   

        dup2(write_pipe.read_fd(), STDIN_FILENO);
        dup2(read_pipe.write_fd(), STDOUT_FILENO);

        write_pipe.close(); 
        read_pipe.close();

        if (with_path) {
          if (envp != 0) result = execvpe(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
          else result = execvp(argv[0], const_cast<char* const*>(argv));
        } else {
          if (envp != 0) result = execve(argv[0], const_cast<char* const*>(argv), const_cast<char* const*>(envp));
          else result = execv(argv[0], const_cast<char* const*>(argv));
        }
        
        if (result == -1) {
          // Note: no point writing to stdout here, it has been redirected
          std::cerr << "Error: Failed to launch program" << std::endl;
          exit(1);
        }

      // In parent process
      } else {

        close(write_pipe.read_fd());
        close(read_pipe.write_fd());

        write_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char> >(new __gnu_cxx::stdio_filebuf<char>(write_pipe.write_fd(), std::ios::out));
        read_buf = std::unique_ptr<__gnu_cxx::stdio_filebuf<char> >(new __gnu_cxx::stdio_filebuf<char>(read_pipe.read_fd(), std::ios::in));

        stdin.rdbuf(write_buf.get());
        stdout.rdbuf(read_buf.get());
      }
    }
    
    // Send end-of-file to stdin of child process
    void send_eof() { write_buf->close(); }
    
    // Wait for child process to end
    int wait() {
      int status;
      waitpid(child_pid, &status, 0);
      return status;
    }
};

#endif //_SPAWN_H_
