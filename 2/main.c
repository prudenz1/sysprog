#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const char* name;
    const char** argv;
    int argc;
    redirect* redirect;
} command;

typedef struct
{
    const char* file;
    bool appending;
} redirect;


int take_spaces(char* str, int start, int end)
{
    int index = start;
    while (index < end && (str[index] == ' ' || str[index] == '\t'))
    {
        index++;
    }
    return index;
}

char* take_token(char* str, int* start, int end)
{
    int index = *start;

    int token_len = 0;
    int token_start = index;

    // printf("tok_start: [%c] ", str[index]);

    if (str[index] == '"' || str[index] == '\'')
    {
        char quote = str[index];
        token_start++;
        index++;
        while (str[index] != quote)
        {
            if (str[index] == '\\')
            {
                index++;
            }
            index++;
            token_len++;

            if (index == end)
            {
                printf("Error: unterminated %c.\n", quote);
                exit(EXIT_FAILURE);
            }
        }
        token_len--;
    }
    else
    {
        while (index < end && str[index] != ' ')
        {
            if (str[index] == '\\')
            {
                index++;
            }
            index++;
            token_len++;
        }
    }

    char* token = calloc(token_len + 1, sizeof(char));
    int token_index = 0;
    int token_end = index;
    index = token_start;
    while (index < token_end)
    {
        if (str[index] == '\\' && str[index + 1] != 'n')
        {
            index++;
        }
        token[token_index++] = str[index++];
    }

    token[token_index] = '\0';

    // printf("tok: [%s]\n", token);

    *start = index + 1;

    return token;
}

command** take_line(int* commands_count, bool* break_loop)
{
    int line_len = 64;
    char* line = calloc(line_len, sizeof(char));
    int index = 0;
    char c;

    bool escaped = false;
    bool in_quote = false;
    char quote = 'a';

    while (true)
    {
        c = getchar();

        if (c == EOF)
        {
            *break_loop = true;
            break;
        }

        if (c == '\n')
        {
            if (escaped)
            {
                index--;
                escaped = false;
                continue;
            }
            else if (!in_quote)
            {
                break;
            }
        }
        if (escaped)
        {
            escaped = false;
        }
        else if (c == '\\')
        {
            escaped = true;
        }
        else if (c == '#')
        {
            break;
        }
        else if (in_quote && c == quote)
        {
            in_quote = false;
        }
        else if (!in_quote && (c == '"' || c == '\''))
        {
            quote = c;
            in_quote = true;
        }
        line[index++] = c;
        if (index == line_len)
        {
            line_len *= 2;
            line = realloc(line, sizeof(char) * line_len);
        }
    }

    line[index] = '\0';
    line_len = index;

    // printf("[%s]\n", line);

    int offset = 0;

    index = 0;
    in_quote = false;
    quote = 'a';
    escaped = false;

    int commands_len = 2;
    int commands_index = 0;
    command** cmds = calloc(commands_len, sizeof(command*));

    while (line[index] != '\0')
    {
        if (escaped)
        {
            escaped = false;
        }
        else if (line[index] == '\\')
        {
            escaped = true;
        }
        else if (in_quote && line[index] == quote)
        {
            in_quote = false;
        }
        else if (!in_quote && (line[index] == '"' || line[index] == '\''))
        {
            in_quote = true;
            quote = line[index];
        }
        else if (!in_quote && line[index] == '|')
        {
            cmds[commands_index++] = take_command(line, offset, index);

            if (commands_index == commands_len)
            {
                commands_len *= 2;
                cmds = realloc(cmds, sizeof(command*) * commands_len);
            }
            offset = index + 1;
        }
        else if (!in_quote && line[index] == '>')
        {
            bool appending = false;
            cmds[commands_index++] = take_command(line, offset, index);

            if (commands_index == commands_len)
            {
                commands_len *= 2;
                cmds = realloc(cmds, sizeof(command*) * commands_len);
            }
            if (line[index + 1] == '>')
            {
                appending = true;
                index++;
            }

            index = take_spaces(line, index + 1, line_len);
            if (index == line_len)
            {
                printf("Error: Invalid use of >.\n");
                exit(EXIT_FAILURE);
            }

            cmds[commands_index - 1]->redirect = calloc(1, sizeof(redirect));
            cmds[commands_index - 1]->redirect->file = take_token(line, &index, line_len);
            cmds[commands_index - 1]->redirect->appending = appending;
            offset = index;
        }
        index++;
    }

    if (offset < line_len)
    {
        cmds[commands_index++] = take_command(line, offset, index);
        if (commands_index == commands_len)
        {
            commands_len *= 2;
            cmds = realloc(cmds, sizeof(command*) * commands_len);
        }
    }

    cmds[commands_index] = NULL;
    *commands_count = commands_index;

    return cmds;
}

command* take_command(char* str, int start, int end)
{
    command* cmd = calloc(1, sizeof(command));

    int offset = take_spaces(str, start, end);
    cmd->name = take_token(str, &offset, end);

    int argc = 0;
    int args_offset = offset;
    while (offset < end)
    {
        offset = take_spaces(str, offset, end);
        if (offset == end)
        {
            break;
        }

        char* tmp = take_token(str, &offset, end);
        free(tmp);
        argc++;
    }

    cmd->argc = argc;
    cmd->argv = calloc(argc + 2, sizeof(char*));

    offset = args_offset;
    cmd->argv[0] = cmd->name;
    argc = 1;
    while (argc <= cmd->argc)
    {
        offset = take_spaces(str, offset, end);
        cmd->argv[argc++] = take_token(str, &offset, end);
    }
    cmd->argv[argc] = NULL;
    cmd->redirect = NULL;

    return cmd;
}

int main()
{
    bool break_loop = false;

    while (!break_loop)
    {
        int com_num = 0;

        command** cmds = take_line(&com_num, &break_loop);
        int pids[com_num];

        int p_start[2];
        int p_end[2];

        if (com_num > 1)
        {
            pipe(p_start);
        }

        for (int i = 0; i < com_num; i++)
        {
            if (i != com_num - 1)
            {
                pipe(p_end);
            }
            if (strcmp(cmds[i]->name, "cd") == 0)
            {
                if (chdir(cmds[i]->argv[1]) == -1)
                {
                    printf("cd: no such file or directory: %s\n", cmds[i]->argv[1]);
                }
                continue;
            }
            else if (strcmp(cmds[i]->name, "exit") == 0 && com_num == 1)
            {
                if (cmds[i]->argc > 0)
                {
                    exit(atol(cmds[i]->argv[1]));
                }
                else
                {
                    exit(0);
                }
                break;
            }

            pids[i] = fork();

            if (pids[i] == 0)
            {
                const char* const* const_argv = (const char* const*)cmds[i]->argv;
                char* const* argv = (char* const*)const_argv;

                if (cmds[i]->redirect)
                {
                    int fd;

                    if (cmds[i]->redirect->appending)
                    {
                        fd = open(cmds[i]->redirect->file, O_WRONLY | O_CREAT | O_APPEND,
                            S_IRUSR | S_IWUSR);
                    }
                    else
                    {
                        fd = open(cmds[i]->redirect->file, O_WRONLY | O_CREAT | O_TRUNC,
                            S_IRUSR | S_IWUSR);
                    }

                    if (fd == -1)
                    {
                        printf("Error: redirect failed.\n");
                        exit(EXIT_FAILURE);
                    }

                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                if (i > 0)
                {
                    dup2(p_start[0], STDIN_FILENO);
                    close(p_start[0]);
                    close(p_start[1]);
                }

                if (i != com_num - 1)
                {
                    close(p_end[0]);
                    dup2(p_end[1], STDOUT_FILENO);
                    close(p_end[1]);
                }

                execvp(cmds[i]->name, argv);

                exit(EXIT_FAILURE);
            }
            else
            {
                if (i > 0)
                {
                    close(p_start[0]);
                    close(p_start[1]);
                }

                if (i != com_num - 1)
                {
                    p_start[0] = p_end[0];
                    p_start[1] = p_end[1];
                }
            }
        }

        if (com_num > 1)
        {
            close(p_start[0]);
            close(p_start[1]);
        }

        for (int i = 0; i < com_num; i++)
        {
            int status;
            waitpid(pids[i], &status, 0);
        }
    }

    return 0;
}