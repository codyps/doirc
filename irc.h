#ifndef IRC_H_
#define IRC_H_

enum irc_num_cmds {
#define RPL(name, value) RPL_##name = value,
#include "irc_spec.h"
#undef RPL
};

static const char *irc_num_cmds[] = {
#define RPL(name, value) [value] = #name,
#include "irc_spec.h"
#undef RPL
};

#endif
