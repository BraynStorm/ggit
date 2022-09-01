#include <Windows.h>

#include "ggit-git.h"


static int
ggit_git_run_vec(
    struct ggit_vector* cmd,
    struct ggit_vector* out,
    struct ggit_vector* err
)
{

#define zero \
    {        \
        0    \
    }
    int result = 1;

    HANDLE stdin_pipe_read;
    HANDLE stdin_pipe_write;
    HANDLE stdout_pipe_read;
    HANDLE stdout_pipe_write;
    HANDLE stderr_pipe_read;
    HANDLE stderr_pipe_write;

    /* Create pipes */
    {
        SECURITY_ATTRIBUTES security = {
            .nLength = sizeof(security),
            .bInheritHandle = 1,
        };

        if (!CreatePipe(&stdin_pipe_read, &stdin_pipe_write, &security, 0))
            goto error_stdin;

        if (!CreatePipe(&stdout_pipe_read, &stdout_pipe_write, &security, 0))
            goto error_stdout;

        if (!CreatePipe(&stderr_pipe_read, &stderr_pipe_write, &security, 0))
            goto error_stderr;

        if (!SetHandleInformation(stdin_pipe_write, HANDLE_FLAG_INHERIT, 0)
            || !SetHandleInformation(stdout_pipe_read, HANDLE_FLAG_INHERIT, 0)
            || !SetHandleInformation(stderr_pipe_read, HANDLE_FLAG_INHERIT, 0))
            goto error_set_handle_info;
    }


    PROCESS_INFORMATION proc_info = zero;
    {
        STARTUPINFO startup_info = {
            .cb = sizeof(startup_info),
            .hStdInput = stdin_pipe_read,
            .hStdOutput = stdout_pipe_write,
            .hStdError = stderr_pipe_write,
            .dwFlags = STARTF_USESTDHANDLES,
        };

        if (!CreateProcessA(
                NULL,
                (char*)cmd->data,
                NULL,
                NULL,
                true,
                0,
                NULL,
                NULL,
                &startup_info,
                &proc_info
            ))
            goto error_create_process;

        CloseHandle(proc_info.hThread);
        CloseHandle(stdin_pipe_read);
        CloseHandle(stdout_pipe_write);
        stdin_pipe_read = INVALID_HANDLE_VALUE;
        stdout_pipe_write = INVALID_HANDLE_VALUE;
    }

    /* Consume STDOUT */
    {
        ggit_vector_reserve(out, 4096);

        /* TODO:
            Improve this code:
                Wait for the pipe's buffer to fill up or the process to die.
                And then read it.

                This should help:
                https://docs.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-getnamedpipeinfo
        */
        DWORD read;
        while (ReadFile(
            stdout_pipe_read,
            out->data + out->size,
            out->capacity - out->size,
            &read,
            0
        )) {
            if (!read)
                break;

            out->size += read;
            if (out->size == out->capacity)
                ggit_vector_reserve_more(out, 4096);
        }

        if (((char*)out->data)[out->size - 1]) {
            char null = 0;
            ggit_vector_push(out, &null);
        }

        /* NOTE: This might be unnecessary. */
        // Wait for it to die.
        WaitForSingleObject(proc_info.hProcess, INFINITE);

        // Get the exit code.
        DWORD exit_code;
        GetExitCodeProcess(proc_info.hProcess, &exit_code);
        result = exit_code;
    }

    // Let it flow down to the cleanup area.

    CloseHandle(proc_info.hProcess);
error_create_process:;
error_set_handle_info:;
    CloseHandle(stderr_pipe_write);
    CloseHandle(stderr_pipe_read);
error_stderr:;
    CloseHandle(stdout_pipe_write);
    CloseHandle(stdout_pipe_read);
error_stdout:;
    CloseHandle(stdin_pipe_write);
    CloseHandle(stdin_pipe_read);
error_stdin:;
    return result;
}