/*
 * WebServ (Anope 2.1) module header.
 *
 * Declares the WebServCore module class and its command subclasses.
 * Provides IRC staff commands (LIST, VIEW, APPROVE, DENY, STATS)
 * that communicate with an external Django API over HTTPS (libcurl).
 *
 * No persistent Serializable state is needed — all data lives in the
 * Django database and is fetched on demand via the API.
 */

#pragma once

#include "module.h"

#include <cstdint>
#include <string>

class WebServCore;

/* ------------------------------------------------------------------ */
/*  Command declarations                                               */
/* ------------------------------------------------------------------ */

class CommandWebServList final : public Command
{
	WebServCore& ws;

public:
	CommandWebServList(Module* creator, WebServCore& parent);
	void Execute(CommandSource& source, const std::vector<Anope::string>& params) override;
	bool OnHelp(CommandSource& source, const Anope::string& subcommand) override;
};

class CommandWebServView final : public Command
{
	WebServCore& ws;

public:
	CommandWebServView(Module* creator, WebServCore& parent);
	void Execute(CommandSource& source, const std::vector<Anope::string>& params) override;
	bool OnHelp(CommandSource& source, const Anope::string& subcommand) override;
};

class CommandWebServApprove final : public Command
{
	WebServCore& ws;

public:
	CommandWebServApprove(Module* creator, WebServCore& parent);
	void Execute(CommandSource& source, const std::vector<Anope::string>& params) override;
	bool OnHelp(CommandSource& source, const Anope::string& subcommand) override;
};

class CommandWebServDeny final : public Command
{
	WebServCore& ws;

public:
	CommandWebServDeny(Module* creator, WebServCore& parent);
	void Execute(CommandSource& source, const std::vector<Anope::string>& params) override;
	bool OnHelp(CommandSource& source, const Anope::string& subcommand) override;
};

class CommandWebServStats final : public Command
{
	WebServCore& ws;

public:
	CommandWebServStats(Module* creator, WebServCore& parent);
	void Execute(CommandSource& source, const std::vector<Anope::string>& params) override;
	bool OnHelp(CommandSource& source, const Anope::string& subcommand) override;
};

class CommandWebServPoll final : public Command
{
	WebServCore& ws;

public:
	CommandWebServPoll(Module* creator, WebServCore& parent);
	void Execute(CommandSource& source, const std::vector<Anope::string>& params) override;
	bool OnHelp(CommandSource& source, const Anope::string& subcommand) override;
};

/* ------------------------------------------------------------------ */
/*  Module class                                                       */
/* ------------------------------------------------------------------ */

class WebServCore final : public Module
{
	Reference<BotInfo> WebServ;

	/* API configuration (set in OnReload from webserv.conf) */
	Anope::string api_url;     /* e.g. https://webdev.irc4fun.net/api/webserv/ */
	Anope::string api_token;   /* Bearer token                                 */
	Anope::string staff_target; /* #channel or "globops"                       */
	Anope::string staff_priv;  /* oper privilege required for commands         */

	bool reply_with_notice = true;

	/* ---- Polling for new requests ---- */
	time_t poll_interval = 60;       /* seconds between polls (0 = disabled) */
	int64_t last_seen_service_id = 0; /* highest service-request ID seen     */
	int64_t last_seen_link_id = 0;    /* highest link-request ID seen        */
	bool first_poll_done = false;     /* skip announcements on first poll    */

	class WebServPollTimer;
	WebServPollTimer* poll_timer = nullptr;

	void PollForNewRequests();

	/* Commands */
	CommandWebServList   command_list;
	CommandWebServView   command_view;
	CommandWebServApprove command_approve;
	CommandWebServDeny   command_deny;
	CommandWebServStats  command_stats;
	CommandWebServPoll   command_poll;

	/* ---- helpers ---- */
	void Reply(CommandSource& source, const Anope::string& msg);
	void Reply(CommandSource& source, const char* text);
	void ReplyF(CommandSource& source, const char* fmt, ...) ATTR_FORMAT(3, 4);

	bool IsStaff(CommandSource& source) const;
	void PageStaff(const Anope::string& msg);

	/*
	 * HTTP helpers (libcurl).
	 * All API calls are blocking — the module is used by opers interactively
	 * so the small latency (<100 ms to localhost) is acceptable.
	 *
	 * Returns true on success (HTTP 2xx), fills `response_body`.
	 * Returns false on error, fills `error_out`.
	 */
	bool HttpGet(const Anope::string& path, std::string& response_body, std::string& error_out);
	bool HttpPost(const Anope::string& path, const std::string& json_body,
	              std::string& response_body, std::string& error_out);

public:
	WebServCore(const Anope::string& modname, const Anope::string& creator);
	~WebServCore() override;

	void OnReload(Configuration::Conf& conf) override;
	EventReturn OnPreHelp(CommandSource& source, const std::vector<Anope::string>& params) override;

	friend class CommandWebServList;
	friend class CommandWebServView;
	friend class CommandWebServApprove;
	friend class CommandWebServDeny;
	friend class CommandWebServStats;
	friend class CommandWebServPoll;
};
