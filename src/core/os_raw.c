/* OperServ core functions
 *
 * (C) 2003-2008 Anope Team
 * Contact us at info@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 *
 * $Id$
 *
 */
/*************************************************************************/

#include "module.h"

int do_raw(User * u);

class OSRaw : public Module
{
 public:
	OSRaw(const std::string &modname, const std::string &creator) : Module(modname, creator)
	{
		Command *c;

		moduleAddAuthor("Anope");
		moduleAddVersion("$Id$");
		moduleSetType(THIRD);

		c = createCommand("RAW", do_raw, is_services_root, OPER_HELP_RAW, -1, -1, -1, -1);
		moduleAddCommand(OPERSERV, c, MOD_UNIQUE);

		if (DisableRaw)
			throw ModuleException("os_raw: Not loading because you probably shouldn't be loading me");
	}
};


/**
 * The /os raw command.
 * @param u The user who issued the command
 * @param MOD_CONT to continue processing other modules, MOD_STOP to stop processing.
 **/
int do_raw(User * u)
{
    char *text = strtok(NULL, "");
    if (!text)
        syntax_error(s_OperServ, u, "RAW", OPER_RAW_SYNTAX);
    else {
        send_cmd(NULL, "%s", text);
        if (WallOSRaw) {
            char *kw = strtok(text, " ");
            while (kw && *kw == ':')
                kw = strtok(NULL, " ");
            ircdproto->SendGlobops(s_OperServ,
                             "\2%s\2 used RAW command for \2%s\2",
                             u->nick,
                             (kw ? kw : "\2non RFC compliant message\2"));
        }
        alog("%s used RAW command for %s", u->nick, text);
    }
    return MOD_CONT;
}

MODULE_INIT("os_raw", OSRaw)
