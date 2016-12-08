/*
 * Anope IRC Services
 *
 * Copyright (C) 2016 Anope Team <team@anope.org>
 *
 * This file is part of Anope. Anope is free software; you can
 * redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software
 * Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "modules/protocol/rfc1459.h"

namespace hybrid
{

class Proto : public IRCDProto
{
	ServiceBot *FindIntroduced();

	void SendSVSKill(const MessageSource &source, User *u, const Anope::string &buf) override;

  public:
	Proto(Module *creator);

	void SendInvite(const MessageSource &source, Channel *c, User *u) override;

	void SendGlobalNotice(ServiceBot *bi, Server *dest, const Anope::string &msg) override;

	void SendGlobalPrivmsg(ServiceBot *bi, Server *dest, const Anope::string &msg) override;

	void SendSQLine(User *, XLine *x) override;

	void SendSGLineDel(XLine *x) override;

	void SendSGLine(User *, XLine *x) override;

	void SendSZLineDel(XLine *x) override;

	void SendSZLine(User *, XLine *x) override;

	void SendAkillDel(XLine *x) override;

	void SendSQLineDel(XLine *x) override;

	void SendJoin(User *u, Channel *c, const ChannelStatus *status) override;

	void SendAkill(User *u, XLine *x) override;

	void SendServer(Server *server) override;

	void SendConnect() override;

	void SendClientIntroduction(User *u) override;

	void SendEOB() override;

	void SendMode(const MessageSource &source, User *u, const Anope::string &buf) override;

	void SendLogin(User *u, NickServ::Nick *na) override;

	void SendLogout(User *u) override;

	void SendChannel(Channel *c) override;

	void SendTopic(const MessageSource &source, Channel *c) override;

	void SendForceNickChange(User *u, const Anope::string &newnick, time_t when) override;

	void SendSVSJoin(const MessageSource &source, User *u, const Anope::string &chan, const Anope::string &) override;

	void SendSVSPart(const MessageSource &source, User *u, const Anope::string &chan, const Anope::string &param) override;

	void SendSVSHold(const Anope::string &nick, time_t t) override;

	void SendSVSHoldDel(const Anope::string &nick) override;

	void SendVhost(User *u, const Anope::string &ident, const Anope::string &host) override;

	void SendVhostDel(User *u) override;

	bool IsIdentValid(const Anope::string &ident) override;
};

class BMask : public IRCDMessage
{
 public:
	BMask(Module *creator) : IRCDMessage(creator, "BMASK", 4) { SetFlag(IRCDMESSAGE_REQUIRE_SERVER); }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class EOB : public IRCDMessage
{
 public:
	EOB(Module *craetor) : IRCDMessage(craetor, "EOB", 0) { SetFlag(IRCDMESSAGE_REQUIRE_SERVER); }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class Join : public rfc1459::Join
{
 public:
	Join(Module *creator) : rfc1459::Join(creator, "JOIN") { }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class Nick : public IRCDMessage
{
 public:
	Nick(Module *creator) : IRCDMessage(creator, "NICK", 2) { SetFlag(IRCDMESSAGE_REQUIRE_USER); }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class Pong : public IRCDMessage
{
 public:
	Pong(Module *creator) : IRCDMessage(creator, "PONG", 0) { SetFlag(IRCDMESSAGE_SOFT_LIMIT); SetFlag(IRCDMESSAGE_REQUIRE_SERVER); }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class ServerMessage : public IRCDMessage
{
 public:
	ServerMessage(Module *creator) : IRCDMessage(creator, "SERVER", 3) { }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class SID : public IRCDMessage
{
 public:
	SID(Module *creator) : IRCDMessage(creator, "SID", 4) { }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class SJoin : public IRCDMessage
{
 public:
	SJoin(Module *creator) : IRCDMessage(creator, "SJOIN", 2) { SetFlag(IRCDMESSAGE_REQUIRE_SERVER); SetFlag(IRCDMESSAGE_SOFT_LIMIT); }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class SVSMode : public IRCDMessage
{
 public:
	SVSMode(Module *creator) : IRCDMessage(creator, "SVSMODE", 3) { SetFlag(IRCDMESSAGE_SOFT_LIMIT); }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class TBurst : public IRCDMessage
{
 public:
	TBurst(Module *creator) : IRCDMessage(creator, "TBURST", 5) { }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class TMode : public IRCDMessage
{
 public:
	TMode(Module *creator) : IRCDMessage(creator, "TMODE", 3) { SetFlag(IRCDMESSAGE_SOFT_LIMIT); }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class UID : public IRCDMessage
{
 public:
	UID(Module *creator) : IRCDMessage(creator, "UID", 10) { SetFlag(IRCDMESSAGE_REQUIRE_SERVER); }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

class CertFP : public IRCDMessage
{
 public:
	CertFP(Module *creator) : IRCDMessage(creator, "CERTFP", 1) { SetFlag(IRCDMESSAGE_REQUIRE_USER); }

	void Run(MessageSource &source, const std::vector<Anope::string> &params) override;
};

} // namespace hybrid
