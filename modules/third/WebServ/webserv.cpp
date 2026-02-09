/*
 * WebServ command implementations + MODULE_INIT.
 *
 * (C) 2025 reverse — Jean Chevronnet
 * IRC: irc.irc4fun.net Port:+6697 (TLS)
 * Channel: #development
 *
 * Commands
 * ────────
 *   LIST    [links] [status]       — List service or link requests
 *   VIEW    <id> [links]           — View a single request in detail
 *   APPROVE <id> [links] [notes]   — Approve a request
 *   DENY    <id> [links] [reason]  — Deny a request
 *   STATS                          — Show request counts
 *
 * All commands require the webserv/staff privilege (configurable).
 * "links" keyword switches between service requests and link requests.
 *
 * Example configuration → webserv.example.conf
 */

/// $LinkerFlags:  -lcrypto -lssl -lcurl -lnlohmann_json

#include "webserv.h"

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <ctime>

/* Convert ISO 8601 timestamp to human-readable "Feb 09, 2026 at 15:20 UTC" */
static std::string FormatTimestamp(const std::string &iso)
{
	if (iso.empty())
		return "N/A";

	struct tm tm = {};
	/* Parse "2026-02-09T15:20:26.945260+00:00" */
	if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%d",
		&tm.tm_year, &tm.tm_mon, &tm.tm_mday,
		&tm.tm_hour, &tm.tm_min, &tm.tm_sec) < 6)
		return iso; /* fallback to raw string */

	tm.tm_year -= 1900;
	tm.tm_mon -= 1;

	char buf[64];
	strftime(buf, sizeof(buf), "%b %d, %Y at %H:%M UTC", &tm);
	return std::string(buf);
}

using json = nlohmann::json;

/* ================================================================== */
/*  LIST                                                               */
/* ================================================================== */

CommandWebServList::CommandWebServList(Module* creator, WebServCore& parent)
	: Command(creator, "webserv/list", 0, 2)
	, ws(parent)
{
	this->SetDesc(_("List pending service/link requests"));
	this->SetSyntax(_("[\037links\037] [\037status\037]"));
}

void CommandWebServList::Execute(CommandSource& source, const std::vector<Anope::string>& params)
{
	if (!this->ws.IsStaff(source))
	{
		this->ws.Reply(source, "Access denied.");
		return;
	}

	/* Determine type and optional status filter */
	bool links = false;
	Anope::string status_filter;

	for (const auto& p : params)
	{
		if (p.equals_ci("links") || p.equals_ci("link"))
			links = true;
		else if (p.equals_ci("pending") || p.equals_ci("approved") || p.equals_ci("denied"))
			status_filter = p.lower();
	}

	/* Build API path */
	std::string path = links ? "links/" : "requests/";
	if (!status_filter.empty())
		path += "?status=" + std::string(status_filter.c_str());

	std::string body, error;
	if (!this->ws.HttpGet(path, body, error))
	{
		this->ws.ReplyF(source, "API error: %s", error.c_str());
		return;
	}

	try
	{
		json data = json::parse(body);
		int total = data.value("total", 0);
		auto results = data.value("results", json::array());

		if (results.empty())
		{
			this->ws.Reply(source, links ? "No link requests found." : "No service requests found.");
			return;
		}

		this->ws.ReplyF(source, "\002%s Requests\002 (%d total):",
			links ? "Link" : "Service", total);

		for (const auto& r : results)
		{
			int id = r.value("id", 0);
			std::string status_label = r.value("status_label", r.value("status", "?"));
			std::string nickname = r.value("nickname", "");
			std::string created = r.value("created_at", "");

			/* Truncate ISO timestamp to date only */
			if (created.size() > 10)
				created = created.substr(0, 10);

			if (links)
			{
				std::string net = r.value("network_name", "");
				this->ws.ReplyF(source,
					"  \002#%d\002  %-10s  nick=\002%s\002  network=\002%s\002  %s",
					id, status_label.c_str(), nickname.c_str(), net.c_str(), created.c_str());
			}
			else
			{
				std::string svc = r.value("service_label", r.value("service", "?"));
				this->ws.ReplyF(source,
					"  \002#%d\002  %-10s  nick=\002%s\002  service=\002%s\002  %s",
					id, status_label.c_str(), nickname.c_str(), svc.c_str(), created.c_str());
			}
		}

		if (total > static_cast<int>(results.size()))
			this->ws.ReplyF(source, "(%d more not shown)", total - static_cast<int>(results.size()));
	}
	catch (const json::exception& e)
	{
		this->ws.ReplyF(source, "Failed to parse API response: %s", e.what());
	}
}

bool CommandWebServList::OnHelp(CommandSource& source, const Anope::string& subcommand)
{
	this->SendSyntax(source);
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Lists service requests from the web portal."));
	this->ws.Reply(source, _("Use \002LIST links\002 to show link requests instead."));
	this->ws.Reply(source, _("Optionally filter by status: \002pending\002, \002approved\002, \002denied\002."));
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Examples:"));
	this->ws.Reply(source, _("  LIST"));
	this->ws.Reply(source, _("  LIST pending"));
	this->ws.Reply(source, _("  LIST links"));
	this->ws.Reply(source, _("  LIST links denied"));
	return true;
}

/* ================================================================== */
/*  VIEW                                                               */
/* ================================================================== */

CommandWebServView::CommandWebServView(Module* creator, WebServCore& parent)
	: Command(creator, "webserv/view", 1, 2)
	, ws(parent)
{
	this->SetDesc(_("View a service/link request in detail"));
	this->SetSyntax(_("\037id\037 [\037links\037]"));
}

void CommandWebServView::Execute(CommandSource& source, const std::vector<Anope::string>& params)
{
	if (!this->ws.IsStaff(source))
	{
		this->ws.Reply(source, "Access denied.");
		return;
	}

	bool links = false;
	Anope::string id_str;

	for (const auto& p : params)
	{
		if (p.equals_ci("links") || p.equals_ci("link"))
			links = true;
		else
			id_str = p;
	}

	if (id_str.empty())
	{
		this->ws.Reply(source, "Please specify a request ID.");
		return;
	}

	/* Strip leading # */
	if (!id_str.empty() && id_str[0] == '#')
		id_str = id_str.substr(1);

	std::string path = (links ? "links/" : "requests/") + std::string(id_str.c_str()) + "/";

	std::string body, error;
	if (!this->ws.HttpGet(path, body, error))
	{
		this->ws.ReplyF(source, "API error: %s", error.c_str());
		return;
	}

	try
	{
		json data = json::parse(body);
		json r = data.value("result", json::object());

		if (r.empty() || !r.contains("id"))
		{
			this->ws.Reply(source, "Request not found.");
			return;
		}

		int id = r.value("id", 0);
		std::string status_label = r.value("status_label", r.value("status", "?"));
		std::string nickname = r.value("nickname", "");
		std::string email = r.value("email", "");
		std::string created = r.value("created_at", "");
		std::string updated = r.value("updated_at", "");
		bool registered = r.value("is_registered", false);
		std::string notes = r.value("admin_notes", "");

		this->ws.ReplyF(source, "\002Request #%d\002  (%s)", id,
			links ? "Link" : "Service");
		this->ws.ReplyF(source, "  Status:     %s", status_label.c_str());
		this->ws.ReplyF(source, "  Nickname:   %s", nickname.c_str());
		this->ws.ReplyF(source, "  Email:      %s", email.c_str());
		this->ws.ReplyF(source, "  Registered: %s", registered ? "Yes" : "No");

		if (links)
		{
			std::string network = r.value("network_name", "");
			std::string network_rr = r.value("network_rr", "");
			std::string network_desc = r.value("network_description", "");
			std::string server_ip = r.value("server_ip", "");
			std::string ircd = r.value("ircd_version", "");
			std::string location = r.value("location", "");
			std::string reason = r.value("reason", "");
			std::string staff = r.value("staff_list", "");

			if (!network.empty())
				this->ws.ReplyF(source, "  Network:    %s", network.c_str());
			if (!network_rr.empty())
				this->ws.ReplyF(source, "  DNS (RR):   %s", network_rr.c_str());
			if (!network_desc.empty())
				this->ws.ReplyF(source, "  Desc:       %s", network_desc.c_str());
			if (!server_ip.empty())
				this->ws.ReplyF(source, "  Server IP:  %s", server_ip.c_str());
			if (!ircd.empty())
				this->ws.ReplyF(source, "  IRCd:       %s", ircd.c_str());
			if (!location.empty())
				this->ws.ReplyF(source, "  Location:   %s", location.c_str());
			if (!reason.empty())
				this->ws.ReplyF(source, "  Reason:     %s", reason.c_str());
			if (!staff.empty())
				this->ws.ReplyF(source, "  Net Staff:  %s", staff.c_str());
		}
		else
		{
			std::string svc = r.value("service_label", r.value("service", "?"));
			bool reg7 = r.value("registered_7days", false);
			std::string networks = r.value("networks", "");
			std::string contact = r.value("contact_email", "");
			std::string referral = r.value("referral", "");

			this->ws.ReplyF(source, "  Service:    %s", svc.c_str());
			this->ws.ReplyF(source, "  Reg >7 days: %s", reg7 ? "Yes" : "No");
			if (!networks.empty())
				this->ws.ReplyF(source, "  Networks:   %s", networks.c_str());
			if (!contact.empty())
				this->ws.ReplyF(source, "  Contact:    %s", contact.c_str());
			if (!referral.empty())
				this->ws.ReplyF(source, "  Referral:   %s", referral.c_str());
		}

		this->ws.ReplyF(source, "  Created:    %s", FormatTimestamp(created).c_str());
		this->ws.ReplyF(source, "  Updated:    %s", FormatTimestamp(updated).c_str());

		if (!notes.empty())
			this->ws.ReplyF(source, "  Notes:      %s", notes.c_str());
	}
	catch (const json::exception& e)
	{
		this->ws.ReplyF(source, "Failed to parse API response: %s", e.what());
	}
}

bool CommandWebServView::OnHelp(CommandSource& source, const Anope::string& subcommand)
{
	this->SendSyntax(source);
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Shows full details of a single request."));
	this->ws.Reply(source, _("Use \002links\002 keyword for link requests."));
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Examples:"));
	this->ws.Reply(source, _("  VIEW 5"));
	this->ws.Reply(source, _("  VIEW #3 links"));
	return true;
}

/* ================================================================== */
/*  APPROVE                                                            */
/* ================================================================== */

CommandWebServApprove::CommandWebServApprove(Module* creator, WebServCore& parent)
	: Command(creator, "webserv/approve", 1, 3)
	, ws(parent)
{
	this->SetDesc(_("Approve a service/link request"));
	this->SetSyntax(_("\037id\037 [\037links\037] [\037notes\037]"));
}

void CommandWebServApprove::Execute(CommandSource& source, const std::vector<Anope::string>& params)
{
	if (!this->ws.IsStaff(source))
	{
		this->ws.Reply(source, "Access denied.");
		return;
	}

	bool links = false;
	Anope::string id_str;
	Anope::string notes;

	for (size_t i = 0; i < params.size(); ++i)
	{
		const auto& p = params[i];
		if (p.equals_ci("links") || p.equals_ci("link"))
		{
			links = true;
		}
		else if (id_str.empty())
		{
			id_str = p;
		}
		else
		{
			/* Remaining params are notes */
			for (size_t j = i; j < params.size(); ++j)
			{
				if (params[j].equals_ci("links") || params[j].equals_ci("link"))
				{
					links = true;
					continue;
				}
				if (!notes.empty())
					notes += " ";
				notes += params[j];
			}
			break;
		}
	}

	if (id_str.empty())
	{
		this->ws.Reply(source, "Please specify a request ID.");
		return;
	}

	if (!id_str.empty() && id_str[0] == '#')
		id_str = id_str.substr(1);

	std::string path = (links ? "links/" : "requests/")
	                    + std::string(id_str.c_str()) + "/status/";

	/* Build JSON body */
	json body_json;
	body_json["status"] = "approved";
	body_json["changed_by"] = std::string(source.GetNick().c_str());
	if (!notes.empty())
		body_json["admin_notes"] = std::string(notes.c_str());

	std::string response, error;
	if (!this->ws.HttpPost(path, body_json.dump(), response, error))
	{
		this->ws.ReplyF(source, "API error: %s", error.c_str());
		return;
	}

	try
	{
		json data = json::parse(response);
		std::string msg = data.value("message", "Approved.");
		json r = data.value("result", json::object());
		int id = r.value("id", 0);
		std::string nickname = r.value("nickname", "");

		this->ws.ReplyF(source, "\002#%d\002 (%s): %s",
			id, nickname.c_str(), msg.c_str());

		/* Notify staff channel */
		Anope::string page_msg = Anope::Format("[WebServ] \002#%d\002 (%s %s) \002APPROVED\002 by %s%s",
			id, links ? "link" : "service", nickname.c_str(),
			source.GetNick().c_str(),
			notes.empty() ? "" : Anope::Format(" — %s", notes.c_str()).c_str());
		this->ws.PageStaff(page_msg);
	}
	catch (const json::exception& e)
	{
		this->ws.ReplyF(source, "Failed to parse API response: %s", e.what());
	}
}

bool CommandWebServApprove::OnHelp(CommandSource& source, const Anope::string& subcommand)
{
	this->SendSyntax(source);
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Approves a pending request by ID."));
	this->ws.Reply(source, _("Use \002links\002 keyword for link requests."));
	this->ws.Reply(source, _("Optionally add notes that are recorded in the admin log."));
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Examples:"));
	this->ws.Reply(source, _("  APPROVE 5"));
	this->ws.Reply(source, _("  APPROVE #3 links Looks good"));
	this->ws.Reply(source, _("  APPROVE 7 Account verified"));
	return true;
}

/* ================================================================== */
/*  DENY                                                               */
/* ================================================================== */

CommandWebServDeny::CommandWebServDeny(Module* creator, WebServCore& parent)
	: Command(creator, "webserv/deny", 1, 3)
	, ws(parent)
{
	this->SetDesc(_("Deny a service/link request"));
	this->SetSyntax(_("\037id\037 [\037links\037] [\037reason\037]"));
}

void CommandWebServDeny::Execute(CommandSource& source, const std::vector<Anope::string>& params)
{
	if (!this->ws.IsStaff(source))
	{
		this->ws.Reply(source, "Access denied.");
		return;
	}

	bool links = false;
	Anope::string id_str;
	Anope::string reason;

	for (size_t i = 0; i < params.size(); ++i)
	{
		const auto& p = params[i];
		if (p.equals_ci("links") || p.equals_ci("link"))
		{
			links = true;
		}
		else if (id_str.empty())
		{
			id_str = p;
		}
		else
		{
			for (size_t j = i; j < params.size(); ++j)
			{
				if (params[j].equals_ci("links") || params[j].equals_ci("link"))
				{
					links = true;
					continue;
				}
				if (!reason.empty())
					reason += " ";
				reason += params[j];
			}
			break;
		}
	}

	if (id_str.empty())
	{
		this->ws.Reply(source, "Please specify a request ID.");
		return;
	}

	if (!id_str.empty() && id_str[0] == '#')
		id_str = id_str.substr(1);

	std::string path = (links ? "links/" : "requests/")
	                    + std::string(id_str.c_str()) + "/status/";

	json body_json;
	body_json["status"] = "denied";
	body_json["changed_by"] = std::string(source.GetNick().c_str());
	if (!reason.empty())
		body_json["admin_notes"] = std::string(reason.c_str());

	std::string response, error;
	if (!this->ws.HttpPost(path, body_json.dump(), response, error))
	{
		this->ws.ReplyF(source, "API error: %s", error.c_str());
		return;
	}

	try
	{
		json data = json::parse(response);
		std::string msg = data.value("message", "Denied.");
		json r = data.value("result", json::object());
		int id = r.value("id", 0);
		std::string nickname = r.value("nickname", "");

		this->ws.ReplyF(source, "\002#%d\002 (%s): %s",
			id, nickname.c_str(), msg.c_str());

		Anope::string page_msg = Anope::Format("[WebServ] \002#%d\002 (%s %s) \002DENIED\002 by %s%s",
			id, links ? "link" : "service", nickname.c_str(),
			source.GetNick().c_str(),
			reason.empty() ? "" : Anope::Format(" — %s", reason.c_str()).c_str());
		this->ws.PageStaff(page_msg);
	}
	catch (const json::exception& e)
	{
		this->ws.ReplyF(source, "Failed to parse API response: %s", e.what());
	}
}

bool CommandWebServDeny::OnHelp(CommandSource& source, const Anope::string& subcommand)
{
	this->SendSyntax(source);
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Denies a pending request by ID."));
	this->ws.Reply(source, _("Use \002links\002 keyword for link requests."));
	this->ws.Reply(source, _("Optionally add a reason that is recorded in the admin log."));
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Examples:"));
	this->ws.Reply(source, _("  DENY 5"));
	this->ws.Reply(source, _("  DENY #3 links Insufficient info"));
	this->ws.Reply(source, _("  DENY 7 Account too new"));
	return true;
}

/* ================================================================== */
/*  STATS                                                              */
/* ================================================================== */

CommandWebServStats::CommandWebServStats(Module* creator, WebServCore& parent)
	: Command(creator, "webserv/stats", 0, 0)
	, ws(parent)
{
	this->SetDesc(_("Show service/link request statistics"));
}

void CommandWebServStats::Execute(CommandSource& source, const std::vector<Anope::string>& params)
{
	if (!this->ws.IsStaff(source))
	{
		this->ws.Reply(source, "Access denied.");
		return;
	}

	std::string body, error;
	if (!this->ws.HttpGet("stats/", body, error))
	{
		this->ws.ReplyF(source, "API error: %s", error.c_str());
		return;
	}

	try
	{
		json data = json::parse(body);
		json sr = data.value("service_requests", json::object());
		json lr = data.value("link_requests", json::object());

		this->ws.Reply(source, "\002WebServ Statistics\002");
		this->ws.Reply(source, " ");

		/* Service requests */
		int sr_total = sr.value("total", 0);
		int sr_pending = sr.value("pending", 0);
		int sr_approved = sr.value("approved", 0);
		int sr_denied = sr.value("denied", 0);

		this->ws.ReplyF(source, "\002Service Requests:\002 %d total  (%d pending, %d approved, %d denied)",
			sr_total, sr_pending, sr_approved, sr_denied);

		if (sr.contains("by_service"))
		{
			json by_svc = sr["by_service"];
			for (auto it = by_svc.begin(); it != by_svc.end(); ++it)
			{
				std::string svc_name = it.key();
				int svc_total = it.value().value("total", 0);
				int svc_pending = it.value().value("pending", 0);
				this->ws.ReplyF(source, "  %s: %d total, %d pending",
					svc_name.c_str(), svc_total, svc_pending);
			}
		}

		/* Link requests */
		int lr_total = lr.value("total", 0);
		int lr_pending = lr.value("pending", 0);
		int lr_approved = lr.value("approved", 0);
		int lr_denied = lr.value("denied", 0);

		this->ws.ReplyF(source, "\002Link Requests:\002  %d total  (%d pending, %d approved, %d denied)",
			lr_total, lr_pending, lr_approved, lr_denied);
	}
	catch (const json::exception& e)
	{
		this->ws.ReplyF(source, "Failed to parse API response: %s", e.what());
	}
}

bool CommandWebServStats::OnHelp(CommandSource& source, const Anope::string& subcommand)
{
	this->SendSyntax(source);
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Shows the count of service and link requests grouped by status."));
	return true;
}

/* ================================================================== */
/*  POLL                                                               */
/* ================================================================== */

CommandWebServPoll::CommandWebServPoll(Module* creator, WebServCore& parent)
	: Command(creator, "webserv/poll", 0, 0)
	, ws(parent)
{
	this->SetDesc(_("Manually trigger a poll for new requests"));
}

void CommandWebServPoll::Execute(CommandSource& source, const std::vector<Anope::string>& params)
{
	if (!this->ws.IsStaff(source))
	{
		this->ws.Reply(source, "Access denied.");
		return;
	}

	this->ws.ReplyF(source, "Polling API... (last_seen: service=#%d, link=#%d, synced=%s)",
		static_cast<int>(this->ws.last_seen_service_id),
		static_cast<int>(this->ws.last_seen_link_id),
		this->ws.first_poll_done ? "yes" : "no");

	this->ws.PollForNewRequests();

	this->ws.ReplyF(source, "Poll complete. (last_seen: service=#%d, link=#%d)",
		static_cast<int>(this->ws.last_seen_service_id),
		static_cast<int>(this->ws.last_seen_link_id));
}

bool CommandWebServPoll::OnHelp(CommandSource& source, const Anope::string& subcommand)
{
	this->SendSyntax(source);
	this->ws.Reply(source, " ");
	this->ws.Reply(source, _("Manually triggers a check for new requests."));
	this->ws.Reply(source, _("Any new requests found will be announced in the staff channel."));
	this->ws.Reply(source, _("This is the same action that runs automatically on the poll timer."));
	return true;
}

/* ================================================================== */

MODULE_INIT(WebServCore)
