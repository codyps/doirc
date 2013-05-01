#ifndef IRC_H_
#define IRC_H_

enum irc_replys {
#define RPL(name, value) RPL_##name = value,
#include "irc_gen.h"
#undef RPL
};

static const char *irc_replys[] = {
#define RPL(name, value) [value] = #name,
#include "irc_gen.h"
#undef RPL
};

#endif
