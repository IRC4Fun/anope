/* Inspired by the original ns_saregister module by Adam.
 * Rewritten for 2.0.x by Maxwell175 (aka MDTech-us_MAN)
 * Date of Original: 14/02/2010
 * Date of This Version: 17/09/2017
 **********************************************
 * Module used in: NickServ
 * Syntax: /msg NickServ SAREGISTER 'Nick' 'Password' 'Email'
 * 
 * This module lets a services operator with the
 * "nickserv/saregister" privileges register a
 * nickname other than their own.
 * Note that this will act as if the user ran the command.
 * This means that either email confirmation or admin
 * approval will be required (if enabled for normal registration).
 **********************************************
 * Completely rewritten for Anope 2.0.
 * Confirmed to work on 2.0.5.
 * Did this stop working in a future version? Let me know on irc.mdtech.us.
 * 
 ************
 * To use, include the following in your configuration:
 * module { name = "ns_saregister" }
 * command { service = "NickServ"; name = "SAREGISTER"; command = "nickserv/saregister"; permission = "nickserv/saregister"; }
 *
 * Don't forget to give your service opers the "nickserv/saregister" permission!
 ************
*/

#include "module.h"

static bool SendRegmail(User *u, const NickAlias *na, BotInfo *bi);

class CommandNSSARegister : public Command
{
 public:
	CommandNSSARegister(Module *creator) : Command(creator, "nickserv/saregister", 3, 3)
	{
		this->SetDesc(_("Registers another user."));
		this->SetSyntax(_("\037nickname\037 \037password\037 \037email\037"));
	}

	void Execute(CommandSource &source, const std::vector<Anope::string> &params) anope_override
	{
		User *u = 0;
		Anope::string u_nick = params[0];
		size_t nicklen = u_nick.length();
		Anope::string pass = params[1];
		Anope::string email = params[2];
		
		// ** Copied from ns_register.cpp ** - If this module breaks in future versions, it is most likely due to a change in the main registration code.
		const Anope::string &nsregister = Config->GetModule(this->owner)->Get<const Anope::string>("registration");

		if (Anope::ReadOnly)
		{
			source.Reply(_("Sorry, nickname registration is temporarily disabled."));
			return;
		}

		if (nsregister.equals_ci("disable"))
		{
			source.Reply(_("Registration is currently disabled."));
			return;
		}

		time_t nickregdelay = Config->GetModule(this->owner)->Get<time_t>("nickregdelay");
		time_t reg_delay = Config->GetModule("nickserv")->Get<time_t>("regdelay");
		if (u && !u->HasMode("OPER") && nickregdelay && Anope::CurTime - u->timestamp < nickregdelay)
		{
			source.Reply(_("You must have been using this nick for at least %d seconds to register."), nickregdelay);
			return;
		}

		/* Prevent "Guest" nicks from being registered. -TheShadow */

		/* Guest nick can now have a series of between 1 and 7 digits.
		 *   --lara
		 */
		const Anope::string &guestnick = Config->GetModule("nickserv")->Get<const Anope::string>("guestnickprefix", "Guest");
		if (nicklen <= guestnick.length() + 7 && nicklen >= guestnick.length() + 1 && !u_nick.find_ci(guestnick) && u_nick.substr(guestnick.length()).find_first_not_of("1234567890") == Anope::string::npos)
		{
			source.Reply(NICK_CANNOT_BE_REGISTERED, u_nick.c_str());
			return;
		}

		if (!IRCD->IsNickValid(u_nick))
		{
			source.Reply(NICK_CANNOT_BE_REGISTERED, u_nick.c_str());
			return;
		}

		if (BotInfo::Find(u_nick, true))
		{
			source.Reply(NICK_CANNOT_BE_REGISTERED, u_nick.c_str());
			return;
		}

		if (Config->GetModule("nickserv")->Get<bool>("restrictopernicks"))
			for (unsigned i = 0; i < Oper::opers.size(); ++i)
			{
				Oper *o = Oper::opers[i];

				if (!source.IsOper() && u_nick.find_ci(o->name) != Anope::string::npos)
				{
					source.Reply(NICK_CANNOT_BE_REGISTERED, u_nick.c_str());
					return;
				}
			}

		unsigned int passlen = Config->GetModule("nickserv")->Get<unsigned>("passlen", "32");

		if (Config->GetModule("nickserv")->Get<bool>("forceemail", "yes") && email.empty())
			this->OnSyntaxError(source, "");
		else if (u && Anope::CurTime < u->lastnickreg + reg_delay)
			source.Reply(_("Please wait %d seconds before using the REGISTER command again."), (u->lastnickreg + reg_delay) - Anope::CurTime);
		else if (NickAlias::Find(u_nick) != NULL)
			source.Reply(NICK_ALREADY_REGISTERED, u_nick.c_str());
		else if (pass.equals_ci(u_nick) || (Config->GetBlock("options")->Get<bool>("strictpasswords") && pass.length() < 5))
			source.Reply(MORE_OBSCURE_PASSWORD);
		else if (pass.length() > passlen)
			source.Reply(PASSWORD_TOO_LONG, passlen);
		else if (!email.empty() && !Mail::Validate(email))
			source.Reply(MAIL_X_INVALID, email.c_str());
		else
		{
			NickCore *nc = new NickCore(u_nick);
			NickAlias *na = new NickAlias(u_nick, nc);
			Anope::Encrypt(pass, nc->pass);
			if (!email.empty())
				nc->email = email;

			if (u)
			{
				na->last_usermask = u->GetIdent() + "@" + u->GetDisplayedHost();
				na->last_realname = u->realname;
			}
			else
				na->last_realname = source.GetNick();

			Log(LOG_COMMAND, source, this) << "to register " << na->nick << " (email: " << (!na->nc->email.empty() ? na->nc->email : "none") << ")";

			if (na->nc->GetAccessCount())
				source.Reply(_("Nickname \002%s\002 registered under your user@host-mask: %s"), u_nick.c_str(), na->nc->GetAccess(0).c_str());
			else
				source.Reply(_("Nickname \002%s\002 registered."), u_nick.c_str());

			Anope::string tmp_pass;
			if (Anope::Decrypt(na->nc->pass, tmp_pass) == 1)
				source.Reply(_("Your password is \002%s\002 - remember this for later use."), tmp_pass.c_str());

			if (nsregister.equals_ci("admin"))
			{
				nc->Extend<bool>("UNCONFIRMED");
			}
			else if (nsregister.equals_ci("mail"))
			{
				if (!email.empty())
				{
					nc->Extend<bool>("UNCONFIRMED");
					SendRegmail(NULL, na, source.service);
				}
			}

			FOREACH_MOD(OnNickRegister, (source.GetUser(), na, pass));

			if (u)
			{
				// This notifies the user that if their registration is unconfirmed
				u->Identify(na);
				u->lastnickreg = Anope::CurTime;
			}
			else if (nc->HasExt("UNCONFIRMED"))
			{
				if (nsregister.equals_ci("admin"))
					source.Reply(_("All new accounts must be validated by an administrator. Please wait for your registration to be confirmed."));
				else if (nsregister.equals_ci("mail"))
					source.Reply(_("Your email address is not confirmed. To confirm it, follow the instructions that were emailed to you."));
			}
		}
		return;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand) anope_override
	{
		this->SendSyntax(source);
			source.Reply(" ");
			source.Reply(_("This module lets a services operator with the\n"
					"nickserv/saregister privileges register a\n"
					"nickname other than their own.\n"
					"Note that this will act as if the user ran the command.\n"
					"This means that either email confirmation or admin\n"
					"approval will be required (if enabled for normal registration)."));
		return true;
	}
};

class NSSARegister : public Module
{
	CommandNSSARegister commandnssaregister;

 public:
	NSSARegister(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, THIRD),
		commandnssaregister(this)
	{
		this->SetAuthor("Maxwell175");
	}
};

// ** Copied from ns_register.cpp **
static bool SendRegmail(User *u, const NickAlias *na, BotInfo *bi)
{
	NickCore *nc = na->nc;

	Anope::string *code = na->nc->GetExt<Anope::string>("passcode");
	if (code == NULL)
	{
		code = na->nc->Extend<Anope::string>("passcode");
		*code = Anope::Random(9);
	}

	Anope::string subject = Language::Translate(na->nc, Config->GetBlock("mail")->Get<const Anope::string>("registration_subject").c_str()),
		message = Language::Translate(na->nc, Config->GetBlock("mail")->Get<const Anope::string>("registration_message").c_str());

	subject = subject.replace_all_cs("%n", na->nick);
	subject = subject.replace_all_cs("%N", Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));
	subject = subject.replace_all_cs("%c", *code);

	message = message.replace_all_cs("%n", na->nick);
	message = message.replace_all_cs("%N", Config->GetBlock("networkinfo")->Get<const Anope::string>("networkname"));
	message = message.replace_all_cs("%c", *code);

	return Mail::Send(u, nc, bi, subject, message);
}

MODULE_INIT(NSSARegister)

