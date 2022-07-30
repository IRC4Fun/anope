#include "module.h"

#define AUTHOR "Gottem"
#define VERSION "1.1"
#define INVALID_NICKNAME "This nickname is invalid for use with standard RFC 2812"

class CommandNSSaGroup : public Command {
	public:
		CommandNSSaGroup(Module *creator) : Command(creator, "nickserv/sagroup", 2, 2) { }

		void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override {
			const Anope::string &nick = params[0];
			const Anope::string &target = params[1];
			const Anope::string &guestnick = Config->GetModule("nickserv")->Get<const Anope::string>("guestnickprefix", "Guest");
			size_t nicklen;
			unsigned maxaliases, i;
			Anope::string last_uhmask, last_gecos;
			NickCore *ncsource;
			NickAlias *na, *natarget, *nasource;
			Oper *sourceOper, *o;
			bool nickOper, targetOper;
			User *u;

			if(Anope::ReadOnly) {
				source.Reply(_("Sorry, nickname grouping is temporarily disabled."));
				return;
			}
			if(!IRCD->IsNickValid(nick)) {
				source.Reply(NICK_CANNOT_BE_REGISTERED, nick.c_str());
				return;
			}
			if(BotInfo::Find(nick, true)) {
				source.Reply(NICK_CANNOT_BE_REGISTERED, nick.c_str());
				return;
			}
			if(isdigit(nick[0]) || nick[0] == '-') {
				source.Reply(INVALID_NICKNAME);
				return;
			}

			nicklen = nick.length();
			if(nicklen <= guestnick.length() + 7 && nicklen >= guestnick.length() + 1 && !nick.find_ci(guestnick) && nick.substr(guestnick.length()).find_first_not_of("1234567890") == Anope::string::npos) {
				source.Reply(NICK_CANNOT_BE_REGISTERED, nick.c_str());
				return;
			}

			if(!(natarget = NickAlias::Find(target)) || !natarget->nc) {
				source.Reply(NICK_X_NOT_REGISTERED, target.c_str());
				return;
			}
			if(natarget->nc->HasExt("NS_SUSPENDED")) {
				source.Reply(NICK_X_SUSPENDED, natarget->nick.c_str());
				return;
			}
			if((na = NickAlias::Find(nick)) && *natarget->nc == *na->nc) {
				source.Reply(_("That nick (\002%s\002) is already a member of that group (\002%s\002)"), nick.c_str(), natarget->nick.c_str());
				return;
			}
			if((maxaliases = Config->GetModule(this->owner)->Get<unsigned>("maxaliases")) && natarget->nc->aliases->size() >= maxaliases) {
				source.Reply(_("There are too many nicks in that group (%d >= %d)"), natarget->nc->aliases->size(), maxaliases);
				return;
			}

			ncsource = source.GetAccount();
			sourceOper = NULL;
			if(ncsource) {
				nasource = NickAlias::Find(source.GetNick());
				if(nasource)
					sourceOper = Oper::Find(nasource->nick);
				if(!sourceOper)
					sourceOper = Oper::Find(ncsource->display);
			}
			for(i = 0; i < Oper::opers.size(); ++i) {
				o = Oper::opers[i];
				nickOper = ((na && na->nick.find_ci(o->name) != Anope::string::npos) || (!na && nick.find_ci(o->name) != Anope::string::npos));
				targetOper = (natarget->nick.find_ci(o->name) != Anope::string::npos);
				if((nickOper && sourceOper != o) || (targetOper && sourceOper != o)) {
					source.Reply(_("You cannot use this command to change an operator's group (except your own)"));
					return;
				}
			}

			last_uhmask = "*@*";
			last_gecos = "unknown";
			u = User::Find(nick, true);
			if(u) {
				last_uhmask = u->GetIdent() + "@" + u->GetDisplayedHost();
				last_gecos = u->realname;
			}
			if(na) {
				last_uhmask = na->last_usermask;
				last_gecos = na->last_realname;
				delete na;
			}
			na = new NickAlias(nick, natarget->nc);
			na->time_registered = na->last_seen = Anope::CurTime;
			na->last_usermask = last_uhmask;
			na->last_realname = last_gecos;
			if(u) {
				u->Login(natarget->nc);
				u->lastnickreg = Anope::CurTime;
				FOREACH_MOD(OnNickGroup, (u, natarget));
				FOREACH_MOD(OnNickUpdate, (u));
			}

			Log(LOG_COMMAND, source, this) << "to make " << nick << " join group of " << natarget->nick << " (" << natarget->nc->display << ") (email: " << (!natarget->nc->email.empty() ? natarget->nc->email : "none") << ")";
			source.Reply(_("The nick \2%s\2 is now in the group of \2%s\2"), na->nick.c_str(), natarget->nick.c_str());
		}

		bool OnHelp(CommandSource &source, const Anope::string &subcommand) {
			source.Reply(_("Syntax: \2SAGROUP \37nick\37 \37target\37"));
			source.Reply(_("Allows services admins to make other nicknames join an existing"));
			source.Reply(_("nick group (\37target\37)."));
			source.Reply(_(" "));
			source.Reply(_("It is recommended to use this command with a non-registered"));
			source.Reply(_("\37nick\37 because it will be registered automatically when"));
			source.Reply(_("using this command."));
			return true;
		}

		void OnSyntaxError(CommandSource &source, const Anope::string &subcommand) {
			source.Reply(_("Syntax: \2SAGROUP \37nick\37 \37target\37"));
		}

		void OnServHelp(CommandSource &source) {
			source.Reply(_("\tSAGROUP        Make a nickname join a group"));
		}
};

class CommandNSSaUngroup : public Command {
	public:
		CommandNSSaUngroup(Module *creator) : Command(creator, "nickserv/saungroup", 1, 1) { }

		void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override {
			const Anope::string &nick = params[0];
			NickAlias *na, *nasource;
			NickCore *nc, *oldcore, *ncsource;
			Oper *sourceOper, *o;
			bool nickOper, targetOper;
			unsigned i;
			User *u;

			if(Anope::ReadOnly) {
				source.Reply(_("Sorry, nickname grouping is temporarily disabled."));
				return;
			}
			if(!(na = NickAlias::Find(nick)) || !na->nc) {
				source.Reply(NICK_X_NOT_REGISTERED, nick.c_str());
				return;
			}
			if(na->nc->aliases->size() == 1) {
				source.Reply(_("The nick \2%s\2 is not grouped to anything, so you can't ungroup it"), na->nick.c_str());
				return;
			}

			oldcore = na->nc;

			ncsource = source.GetAccount();
			sourceOper = NULL;
			if(ncsource) {
				nasource = NickAlias::Find(source.GetNick());
				if(nasource)
					sourceOper = Oper::Find(nasource->nick);
				if(!sourceOper)
					sourceOper = Oper::Find(ncsource->display);
			}
			for(i = 0; i < Oper::opers.size(); ++i) {
				o = Oper::opers[i];
				nickOper = (na->nick.find_ci(o->name) != Anope::string::npos);
				targetOper = (oldcore->display.find_ci(o->name) != Anope::string::npos);
				if((nickOper && sourceOper != o) || (targetOper && sourceOper != o)) {
					source.Reply(_("You cannot use this command to change an operator's group (except your own)"));
					return;
				}
			}

			std::vector<NickAlias *>::iterator it = std::find(oldcore->aliases->begin(), oldcore->aliases->end(), na);
			if(it != oldcore->aliases->end())
				oldcore->aliases->erase(it);
			if(na->nick.equals_ci(oldcore->display))
				oldcore->SetDisplay(oldcore->aliases->front());

			nc = new NickCore(na->nick);
			na->nc = nc;
			nc->aliases->push_back(na);
			nc->pass = oldcore->pass;
			if(!oldcore->email.empty())
				nc->email = oldcore->email;
				nc->language = oldcore->language;

			u = User::Find(na->nick, true);
			if(u)
				u->Login(nc);

			Log(LOG_COMMAND, source, this) << "to make " << nick << " leave group of " << oldcore->display;
			source.Reply(_("Nick \2%s\2 has been ungrouped from \2%s\2"), na->nick.c_str(), oldcore->display.c_str());
		}

		bool OnHelp(CommandSource &source, const Anope::string &subcommand) {
			source.Reply(_("Syntax: \2SAUNGROUP \37nick\37"));
			source.Reply(_("Allows services admins to make other nicknames leave the group they're in (doesn't \2DROP\2 the nick)"));
			return true;
		}

		void OnSyntaxError(CommandSource &source, const Anope::string &subcommand) {
			source.Reply(_("Syntax: \2SAUNGROUP \37nick\37"));
		}

		void OnServHelp(CommandSource &source) {
			source.Reply(_("\tSAUNGROUP        Make a nickname leave the group it's in (doesn't \2DROP\2 the nick)"));
		}
};

class NSSaGroup : public Module {
	CommandNSSaGroup commandnssagroup;
	CommandNSSaUngroup commandnssaungroup;

	public:
		NSSaGroup(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, THIRD), commandnssagroup(this), commandnssaungroup(this) {
			this->SetAuthor(AUTHOR);
			this->SetVersion(VERSION);
			if(Config->GetModule("nickserv")->Get<bool>("nonicknameownership"))
				throw ModuleException(modname + " can not be used with options:nonicknameownership enabled");
		}
};

MODULE_INIT(NSSaGroup)
