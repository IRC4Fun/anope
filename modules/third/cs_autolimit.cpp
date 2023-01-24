/* ------------------------------------------------------------------------------------------------
 *  Name:               | CS_AUTOLIMIT
 *  Author:             | Fill
 *  Version:            | 0.0.1 (Anope 2.0.1)
 *  Date:               | 02/2015
 * ------------------------------------------------------------------------------------------------
 * This module adds a command to ChanServ (AUTOLIMIT) to automatically manage a channel user limit.
 * (mode +l)
 *
 * This is a rewrite of the original cs_autolimit for Anope 2.0.1
 * ------------------------------------------------------------------------------------------------
 * This program is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 1, or (at your option) any later version.
 * ------------------------------------------------------------------------------------------------
 *  To use, place this in your services.conf):
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ START CONF OPTIONS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
module {
       name = "cs_autolimit";
}
command { service = "ChanServ"; name = "AUTOLIMIT"; command = "chanserv/autolimit"; }
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ END CONF OPTIONS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */

/* Do NOT touch anything below unless you really know what you're getting into.
 * Changing the code without knowing what you're doing can CRASH services and / or the IRCd.
 * You have been warned.
 */

#include "module.h"

using namespace std;

struct ChanAlimitInfo;
typedef map<Anope::string, ChanAlimitInfo *> alimit_map;
static alimit_map alimits_db;

#define MIN_ALIMIT_AMT 1
#define MAX_ALIMIT_AMT 100
#define MIN_ALIMIT_INTERVAL 2
#define MAX_ALIMIT_INTERVAL 300
#define TIMER_INTERVAL 5

struct ChanAlimitInfo : Serializable {
        Anope::string chan_name;
        Anope::string author;
        unsigned amount;
        time_t interval;
        time_t last_set;

        ChanAlimitInfo() : Serializable("ChanAlimitInfo") { }
        ~ChanAlimitInfo() {
                alimit_map::iterator iter = alimits_db.find(chan_name.lower());
                if (iter != alimits_db.end() && iter->second == this)
                        alimits_db.erase(iter);
        }

        void Serialize(Serialize::Data &data) const anope_override
        {
                data["chan"] << chan_name;
                data["author"] << author;
                data["amount"] << amount;
                data["interval"] << interval;
                data["last_set"] << last_set;
        }

        static Serializable* Unserialize(Serializable *obj, Serialize::Data &data)
        {
                Anope::string chan;
                Anope::string chan_std;

                data["chan"] >> chan;

                chan_std = chan.lower();

                ChanAlimitInfo *cai;
                if (obj)
                        cai = anope_dynamic_static_cast<ChanAlimitInfo *>(obj);
                else
                {
                        ChanAlimitInfo* &info = alimits_db[chan_std];
                        if (!info)
                                info = new ChanAlimitInfo();
                        cai = info;
                }

                cai->chan_name = chan;
                data["author"] >> cai->author;
                data["amount"] >> cai->amount;
                data["interval"] >> cai->interval;
                data["last_set"] >> cai->last_set;

                if (!obj)
                        alimits_db[chan_std] = cai;

                return cai;
        }
};

class CommandCSAutoLimit : public Command
{
 public:
        CommandCSAutoLimit(Module *creator, const Anope::string &sname = "chanserv/autolimit") : Command(creator, sname, 1, 4)
        {
                this->SetDesc(_("Automatically manages the channel user limit"));
                this->SetSyntax(_("<channel> SET <amount> <interval>"));
                this->SetSyntax(_("<channel> DEL"));
                this->SetSyntax(_("<channel> SHOW"));
                this->SetSyntax(_("LIST (opers only)"));
        }

        void Execute(CommandSource &source, const vector<Anope::string> &params) anope_override
        {

                if (!params[0].equals_ci("LIST")) {
                        if (params.size() < 2) {
                                this->OnSyntaxError(source, params[0]);
                                return;
                        }
                        if (!IRCD->IsChannelValid(params[0])) {
                                source.Reply(_("Invalid channel name: %s\r\n"), params[0].c_str());
                                return;
                        }
                        ChannelInfo *ci = ChannelInfo::Find(params[0]);
                        if (ci == NULL) {
                                source.Reply(CHAN_X_NOT_REGISTERED, params[0].c_str());
                                return;
                        }
                        bool has_access = AccessCheck(source, ci);
                        if (!has_access) {
                                source.Reply(_("Access denied.\r\n"));
                                return;
                        }
                        else if (params[1].equals_ci("SET")) {
                                if (params.size() < 4) {
                                        this->OnSyntaxError(source, params[0]);
                                        return;
                                }
                                unsigned amount;
                                time_t interval;
                                try {
                                        amount = convertTo<unsigned>(params[2]);
                                        interval = convertTo<time_t>(params[3]);
                                }
                                catch (const ConvertException &)
                                {
                                        this->OnSyntaxError(source, "SET");
                                        return;
                                }
                                if (amount < MIN_ALIMIT_AMT || amount > MAX_ALIMIT_AMT) {
                                        source.Reply(_("Invalid increase amount value. Please specify a value in the range %d-%d\r\n"), MIN_ALIMIT_AMT, MAX_ALIMIT_AMT);
                                        return;
                                }
                                if (interval < MIN_ALIMIT_INTERVAL || interval > MAX_ALIMIT_INTERVAL) {
                                        source.Reply(_("Invalid time interval. Please specify a value in the range %d-%d\r\n"), MIN_ALIMIT_INTERVAL, MAX_ALIMIT_INTERVAL);
                                        return;
                                }

                                DoAlimitSet(source, ci, params[0], amount, interval);

                        } else if (params[1].equals_ci("DEL")) {
                                if (!DoAlimitDel(ci, params[0])) {
                                        source.Reply(_("Channel %s not found on the AUTOLIMIT database.\r\n"), params[0].c_str());
                                        return;
                                } else {
                                        source.Reply(_("Removed %s from the AUTOLIMIT list.\r\n"), params[0].c_str());
                                }
                        } else if (params[1].equals_ci("SHOW")) {
                                alimit_map::const_iterator it = alimits_db.find(params[0].lower());
                                if (it == alimits_db.end()) {
                                        source.Reply(_("Channel %s not found on the AUTOLIMIT database.\r\n"), params[0].c_str());
                                        return;
                                }

                                ReportEntry(source, it->second);

                        } else {
                                this->OnSyntaxError(source, params[0]);
                                return;
                        }
                } else {
                        if (!source.GetUser()->HasMode("OPER")) {
                                source.Reply(_("Access denied.\r\n"));
                                return;
                        }
                        if (alimits_db.begin() == alimits_db.end())
                                source.Reply(_("No entries.\r\n"));
                        for (alimit_map::const_iterator it = alimits_db.begin(); it != alimits_db.end(); it++)
                                ReportEntry(source, it->second);
                }

                ostringstream oss;
                for (vector<Anope::string>::const_iterator it = params.begin(); it != params.end(); it++)
                        oss << *it << " ";
                Log(LOG_ADMIN, source, this) << oss.str();
        }

        bool OnHelp(CommandSource &source, const Anope::string &subcommand) anope_override
        {
                this->SendSyntax(source);
                source.Reply(" \r\n");
                source.Reply(_("This command let's channel admins configure automatic\r\n"
                               "adjustment of the user limit (mode +l). A user is considered\r\n"
                               "to be a channel admin if he/she can modify the channel's access\r\n"
                               "list.\r\n \r\n"
                               "The \002SET\002 command is used to add a new channel to the list\r\n"
                               "of managed channels, or to update a channel's current settings.\r\n"
                               "This command will make services set the channel limit (+l) every\r\n"
                               "<interval> seconds. The limit set is equal to the number of users\r\n"
                               "at that time, plus <amount>.\r\n"
                               "Please note that choosing a large interval or a small amount may result\r\n"
                               "in users being unable to join your channel during or after netsplits.\r\n"
                               "For starters, a reasonable value might be an interval of 30 seconds\r\n"
                               "with an amount of 5 or 10.\r\n \r\n"
                               "\002DEL\002 deletes a channel from the managed channels list. Services\r\n"
                               "will remove the limit mode and stop managing the channel.\r\n \r\n"
                               "\002SHOW\002 is used to see the current settings for a specific channel.\r\n \r\n"
                               "The command \002LIST\002 shows a full list of every channel managed by\r\n"
                               "services and its settings regarding the user limit mode. This command is\r\n"
                               "available for IRCOps only.\r\n \r\n"
                               "IRCOps can use any of these commands in any channel."));
                return true;
        }

private:
        void ReportEntry(CommandSource &source, const ChanAlimitInfo *entry) {
                source.Reply(_("%s (<usercount>+%u every %ld seconds) - Last edited by: %s\r\n"),
                             entry->chan_name.c_str(), entry->amount, entry->interval, entry->author.c_str());
        }

        bool AccessCheck(CommandSource &source, ChannelInfo *ci) {
                return source.GetUser()->HasMode("OPER") || source.AccessFor(ci).HasPriv("chanserv/access/modify");
        }

        void DoAlimitSet(CommandSource &source, ChannelInfo *ci, const Anope::string &chan, unsigned amount, time_t interval) {

                Anope::string chan_name = chan.lower();
                ChanAlimitInfo *&cinfo = alimits_db[chan_name];

                if (cinfo == NULL) {
                        cinfo = new ChanAlimitInfo();
                        cinfo->chan_name = chan;
                }

                cinfo->author = source.GetUser()->nick;
                cinfo->amount = amount;
                cinfo->interval = interval;
                cinfo->last_set = Anope::CurTime;

                alimits_db[chan_name] = cinfo;

                if (ci->c != NULL) {
                        ostringstream oss;
                        oss << ci->c->users.size()+cinfo->amount;
                        ci->c->SetMode(NULL, "LIMIT", oss.str(), false);
                }

                source.Reply(_("Channel user limit for %s will be set to <usercount>+%d every %ld seconds."), chan.c_str(), amount, interval);
        }

        bool DoAlimitDel(ChannelInfo *ci, const Anope::string &chan) {
                alimit_map::iterator it = alimits_db.find(chan.lower());
                if (it == alimits_db.end())
                        return false;
                delete it->second;
                if (ci->c != NULL)
                        ci->c->RemoveMode(NULL, "LIMIT", "", false);
                return true;
        }
};

class SetChanLimitTimer : public Timer
{
public:
        SetChanLimitTimer(Module *this_module, long interval) : Timer(this_module, interval, Anope::CurTime, true) { }

        void Tick(time_t now) anope_override {
                for (alimit_map::const_iterator it = alimits_db.begin(); it != alimits_db.end(); it++) {
                        ChanAlimitInfo *cai = it->second;
                        if (cai->last_set + cai->interval <= Anope::CurTime) {
                                ChannelInfo *ci = ChannelInfo::Find(cai->chan_name);
                                if (ci == NULL || ci->c == NULL)
                                        continue;
                                ostringstream oss;
                                oss << ci->c->users.size()+cai->amount;
                                ci->c->SetMode(NULL, "LIMIT", oss.str(), false);
                                cai->last_set = Anope::CurTime;
                        }
                }
        }
};

class CSAutoLimit : public Module
{
public:
        CSAutoLimit(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator, THIRD), commandcsautolimit(this),
                                                                                  chanlimittimer(this, TIMER_INTERVAL),
                                                                                  alimitinfo_type("ChanAlimitInfo", ChanAlimitInfo::Unserialize)
        {
                this->SetAuthor("Fill <filipe@codinghighway.com>");
                this->SetVersion("0.0.1");
        }

        EventReturn OnChanDrop(CommandSource &source, ChannelInfo *ci) anope_override
        {
                Anope::string chan = ci->name.lower();
                alimit_map::iterator it = alimits_db.find(chan);
                if (it == alimits_db.end())
                        return EVENT_CONTINUE;
                delete it->second;
                if (ci->c != NULL)
                        ci->c->RemoveMode(NULL, "LIMIT", "", false);
                return EVENT_CONTINUE;
        }

        void OnChannelCreate(Channel *c) anope_override
        {
                Anope::string chan = c->name.lower();
                alimit_map::iterator it = alimits_db.find(chan);
                if (it == alimits_db.end())
                        return;
                ostringstream oss;
                oss << c->users.size()+it->second->amount;
                c->SetMode(NULL, "LIMIT", oss.str(), false);
        }

private:
        CommandCSAutoLimit commandcsautolimit;
        SetChanLimitTimer chanlimittimer;
        Serialize::Type alimitinfo_type;
};

MODULE_INIT(CSAutoLimit)
