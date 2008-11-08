/* MemoServ core functions
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

int do_sendall(User * u);
void myMemoServHelp(User * u);

class MSSendAll : public Module
{
 public:
	MSSendAll(const std::string &modname, const std::string &creator) : Module(modname, creator)
	{
		Command *c;

		moduleAddAuthor("Anope");
		moduleAddVersion("$Id$");
		moduleSetType(CORE);
		c = createCommand("SENDALL", do_sendall, is_services_admin, -1, -1, -1, MEMO_HELP_SENDALL, MEMO_HELP_SENDALL);
		moduleAddCommand(MEMOSERV, c, MOD_UNIQUE);
		moduleSetMemoHelp(myMemoServHelp);
	}
};



/**
 * Add the help response to anopes /ms help output.
 * @param u The user who is requesting help
 **/
void myMemoServHelp(User * u)
{
    if (is_services_admin(u)) {
        notice_lang(s_MemoServ, u, MEMO_HELP_CMD_SENDALL);
    }
}

/**
 * The /ms sendall command.
 * @param u The user who issued the command
 * @param MOD_CONT to continue processing other modules, MOD_STOP to stop processing.
 **/
int do_sendall(User * u)
{
    int i, z = 1;
    NickCore *nc;
    char *text = strtok(NULL, "");

    if (readonly) {
        notice_lang(s_MemoServ, u, MEMO_SEND_DISABLED);
        return MOD_CONT;
    } else if (checkDefCon(DEFCON_NO_NEW_MEMOS)) {
        notice_lang(s_MemoServ, u, OPER_DEFCON_DENIED);
        return MOD_CONT;
    } else if (!text) {
        syntax_error(s_MemoServ, u, "SENDALL", MEMO_SEND_SYNTAX);
        return MOD_CONT;
    }


    for (i = 0; i < 1024; i++) {
        for (nc = nclists[i]; nc; nc = nc->next) {
            if (stricmp(u->nick, nc->display) != 0)
                memo_send(u, nc->display, text, z);
        }                       /* /nc */
    }                           /* /i */

    notice_lang(s_MemoServ, u, MEMO_MASS_SENT);
    return MOD_CONT;
}

MODULE_INIT("ms_sendall", MSSendAll)
