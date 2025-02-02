#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <string>
#include <iostream>
#include <sys/wait.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

void child(char * argv [], int read_fd, int write_fd);
void parental_control(int child_read_out_fd, int child_write_in_fd);
ssize_t forward(int read_fd, int write_fd, const char * in_error, const char * out_error);

constexpr static bool debug = true;

const static std::string termination_message {
	"\x01x\0"
	// "buh!\n\0"
};
const static std::string termination_request {
	"press Ctrl-A and then X"
	// "A"
};

void assert_or_exit(bool non_fail_condition, const char * error_message) {
	if (non_fail_condition)
		return;

	perror(error_message);
	exit(1);
}

int main (int argc, char ** argv) {
	/**
	 * numbers are
	 * 	- for pipes: ..._pipe_fd[2] indices
	 * 	- for processes: file descriptors
	 *
	 * parent::stdin 0 -> 1 in_pipe 0  ->  0 child 1  ->  1 out_pipe 0 -> 1 parent::out
	 *                    \<- read_fd   parental_control  write_fd <-/
	 */
	int  in_pipe_fd[2];
	int out_pipe_fd[2];

	assert_or_exit(pipe( in_pipe_fd) == 0,  "in pipe failed.");
	assert_or_exit(pipe(out_pipe_fd) == 0, "out pipe failed.");

	int  in_pipe_read  =  in_pipe_fd[0];
	int  in_pipe_write =  in_pipe_fd[1];
	int out_pipe_read  = out_pipe_fd[0];
	int out_pipe_write = out_pipe_fd[1];


	int child_pid = fork();
	assert_or_exit(child_pid >= 0, "fork failed.");

	if (debug) fprintf(
		stderr,
		"pipes in (%d -> %d), out (%d -> %d) created and forked.\n",
		in_pipe_write,
		in_pipe_read,
		out_pipe_write,
		out_pipe_read
	);

	if (child_pid == 0) {
		child(argv + 1, in_pipe_read, out_pipe_write);
	} else {
		parental_control(out_pipe_read, in_pipe_write);

		if (debug) if (debug) fprintf(
			stderr,
			"waiting for child to terminate.\n"
		);
		// parental_control returned, now wait for child to actually terminate.
		int result;
		int pid = wait(&result);
		assert_or_exit(pid == child_pid, "wait failed.");

		if (debug) if (debug) fprintf(
			stderr,
			"child %d terminated with code %d.\n",
			pid,
			result
		);
		return result;
	}
}

void child(char ** argv, int read_fd, int write_fd) {
	if (debug) fprintf(
		stderr,
		"redirecting for child: stdin from %d, stdout to %d.\n",
		read_fd,
		write_fd
	);

	dup2( read_fd,  STDIN_FILENO);
	dup2(write_fd, STDOUT_FILENO);

	if (debug) fprintf(stderr, "starting subprocess:");
	for (char ** arg = argv; *arg; arg ++) {
		std::cerr << " " << *arg;
	}
	std::cerr << std::endl;

	execvp(argv[0], argv);
	assert_or_exit(false, "exec failed.");
}

// used by forward and parental_control
constexpr ssize_t buffer_length = 4096;
char buffer[buffer_length + 1];

ssize_t forward(int read_fd, int write_fd, const char * in_error, const char * out_error) {
	errno = 0;
	ssize_t bytes_in = read(read_fd, buffer, buffer_length);
	assert_or_exit(bytes_in != -1 || errno != EAGAIN || errno != EINTR, in_error);
	if (debug) fprintf(stderr, "forwarding %d -> %d. bytes = %ld, errno = %d\n", read_fd, write_fd, bytes_in, errno);

	if (bytes_in) {
		int bytes_out = write(write_fd, buffer, bytes_in);
		assert_or_exit(bytes_out == bytes_in, out_error);
	}
	return bytes_in;
}

void configure_non_blocking(int read_fd) {
	int flags = fcntl(read_fd, F_GETFL, 0);
	int errc = fcntl(read_fd, F_SETFL, flags | O_NONBLOCK);

	assert_or_exit(errc != -1, "setting to non-blocking");
}

void parental_control(int child_read_out_fd, int child_write_in_fd) {
	configure_non_blocking(STDIN_FILENO);
	configure_non_blocking(child_read_out_fd);

	bool running = true;
	bool still_has_output = false;

	while (running || still_has_output) {
		// forward input on parent's stdin to child's input pipe
		forward(
			STDIN_FILENO,
			child_write_in_fd,
			"read from parent's stdin failed.",
			"writing to child's in pipe failed."
		);

		// forward output on childs's output pipe to parent's stdout
		ssize_t bytes_child_out = forward(
			child_read_out_fd,
			STDOUT_FILENO,
			"read from child's out pipe failed.",
			"writing to parent's stdout failed."
		);
		still_has_output = bytes_child_out > 0;
		// buffer now contains, what flowed from child to stdout

		// look for termination request from inside qemu
		std::string_view message { buffer, buffer_length };
		if (message.npos != message.find(termination_request)) {
			if (debug) fprintf(
				stderr,
				"found termination_request '%s' in buffer '%s'. "
				"sending in '%s'\n",
				termination_request.c_str(),
				buffer,
				termination_message.c_str()
			);
			// found the message that requests termination
			int bytes_child_in = write(
				child_write_in_fd,
				termination_message.c_str(),
				termination_message.size()
			);
			assert_or_exit(
				bytes_child_in == termination_message.size(),
				"writing termination message to child's in pipe"
			);

			running = false;
		}

		// if the child did not write a lot, don't wait busily, wait a bit
		if (bytes_child_out < buffer_length) {
			usleep(1000000); // sets a max throughput of 1000 * buffer_length / second
		}
	}
}
