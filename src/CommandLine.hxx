/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#ifndef PASSAGE_COMMAND_LINE_HXX
#define PASSAGE_COMMAND_LINE_HXX

#include <string>

struct CommandLine {
    std::string config_path;
};

CommandLine
ParseCommandLine(int argc, char **argv);

#endif
