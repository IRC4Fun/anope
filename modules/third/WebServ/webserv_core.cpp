/*
 * WebServ core — module lifecycle, configuration, HTTP helpers, reply routing.
 *
 * (C) 2025 reverse — Jean Chevronnet
 * IRC: irc.irc4fun.net Port:+6697 (TLS)
 * Channel: #development
 *
 * WebServ module for Anope 2.1
 * Provides IRC commands LIST / VIEW / APPROVE / DENY / STATS that
 * interact with an external Django REST API (service & link requests).
 *
 * /// $LinkerFlags: -lcurl
 */

#include "webserv.h"

#include <cstdarg>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "timers.h"

using json = nlohmann::json;

/* ------------------------------------------------------------------ */
/*  libcurl write callback                                             */
/* ------------------------------------------------------------------ */

static size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	auto* buf = static_cast<std::string*>(userdata);
	const size_t total = size * nmemb;
	buf->append(ptr, total);
	return total;
}

/* ------------------------------------------------------------------ */
/*  HTTP helpers                                                       */
/* ------------------------------------------------------------------ */

bool WebServCore::HttpGet(const Anope::string& path, std::string& response_body, std::string& error_out)
{
	CURL* curl = curl_easy_init();
	if (!curl)
	{
		error_out = "Could not initialise libcurl";
		return false;
	}

	std::string url = std::string(this->api_url.c_str()) + std::string(path.c_str());
	std::string auth = "Authorization: Bearer " + std::string(this->api_token.c_str());

	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, auth.c_str());
	headers = curl_slist_append(headers, "Accept: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Anope-WebServ/1.0");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		error_out = std::string("HTTP request failed: ") + curl_easy_strerror(res);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		return false;
	}

	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (http_code < 200 || http_code >= 300)
	{
		error_out = "HTTP " + std::to_string(http_code);
		return false;
	}
	return true;
}

bool WebServCore::HttpPost(const Anope::string& path, const std::string& json_body,
                           std::string& response_body, std::string& error_out)
{
	CURL* curl = curl_easy_init();
	if (!curl)
	{
		error_out = "Could not initialise libcurl";
		return false;
	}

	std::string url = std::string(this->api_url.c_str()) + std::string(path.c_str());
	std::string auth = "Authorization: Bearer " + std::string(this->api_token.c_str());

	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, auth.c_str());
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, "Accept: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Anope-WebServ/1.0");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		error_out = std::string("HTTP request failed: ") + curl_easy_strerror(res);
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		return false;
	}

	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (http_code < 200 || http_code >= 300)
	{
		error_out = "HTTP " + std::to_string(http_code);
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------ */
/*  Reply helpers (same pattern as HelpServ)                           */
/* ------------------------------------------------------------------ */

void WebServCore::Reply(CommandSource& source, const Anope::string& msg)
{
	if (!this->WebServ.operator bool())
	{
		source.Reply("%s", msg.c_str());
		return;
	}

	User* u = source.GetUser();
	if (!u)
	{
		source.Reply("%s", msg.c_str());
		return;
	}

	Anope::map<Anope::string> tags;
	if (!source.msgid.empty())
		tags["+draft/reply"] = source.msgid;

	LineWrapper lw(Language::Translate(u, msg.c_str()));
	for (Anope::string line; lw.GetLine(line); )
	{
		if (this->reply_with_notice)
			IRCD->SendNotice(*this->WebServ, u->GetUID(), line, tags);
		else
			IRCD->SendPrivmsg(*this->WebServ, u->GetUID(), line, tags);
	}
}

void WebServCore::Reply(CommandSource& source, const char* text)
{
	this->Reply(source, Anope::string(text));
}

void WebServCore::ReplyF(CommandSource& source, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	Anope::string msg = Anope::Format(args, fmt);
	va_end(args);
	this->Reply(source, msg);
}

bool WebServCore::IsStaff(CommandSource& source) const
{
	return source.HasPriv(this->staff_priv);
}

void WebServCore::PageStaff(const Anope::string& msg)
{
	if (!this->WebServ || this->staff_target.empty())
		return;

	if (this->staff_target.equals_ci("globops"))
	{
		IRCD->SendGlobops(*this->WebServ, msg);
		return;
	}

	if (this->staff_target[0] == '#')
	{
		this->WebServ->Join(this->staff_target);
		IRCD->SendPrivmsg(*this->WebServ, this->staff_target, msg);
	}
}

/* ------------------------------------------------------------------ */
/*  Poll timer — checks for new requests periodically                  */
/* ------------------------------------------------------------------ */

class WebServCore::WebServPollTimer final
	: public Timer
{
	WebServCore& ws;

public:
	WebServPollTimer(WebServCore& owner, time_t seconds)
		: Timer(&owner, seconds)  /* repeating */
		, ws(owner)
	{
	}

	bool Tick() override
	{
		if (!Me || !Me->IsSynced())
			return false;  // Add return statement
		this->ws.PollForNewRequests();
		return true;  // Add return statement
	}
};

void WebServCore::PollForNewRequests()
{
	if (this->api_url.empty() || this->api_token.empty())
	{
		Log(LOG_COMMAND) << "[webserv] poll skipped: api_url or api_token not set";
		return;
	}
	if (!this->WebServ)
	{
		Log(LOG_COMMAND) << "[webserv] poll skipped: bot not available";
		return;
	}

	Log(LOG_DEBUG) << "[webserv] poll tick: first_poll_done=" << (this->first_poll_done ? "yes" : "no")
	               << " last_service_id=" << this->last_seen_service_id
	               << " last_link_id=" << this->last_seen_link_id;

	/* --- Check service requests --- */
	{
		std::string body, error;
		if (this->HttpGet("requests/?status=pending&limit=50", body, error))
		{
			try
			{
				json data = json::parse(body);
				auto results = data.value("results", json::array());
				int64_t max_id = this->last_seen_service_id;

				for (const auto& r : results)
				{
					int64_t id = r.value("id", static_cast<int64_t>(0));
					if (id > this->last_seen_service_id)
					{
						if (this->first_poll_done)
						{
							std::string nickname = r.value("nickname", "");
							std::string svc = r.value("service_label", r.value("service", "?"));
							std::string email = r.value("email", "");

							Anope::string msg = Anope::Format(
								"[WebServ] \002New service request #%d\002 - "
								"nick=\002%s\002 service=\002%s\002 email=%s  "
								"[\002VIEW %d\002 | \002APPROVE %d\002 | \002DENY %d\002]",
								static_cast<int>(id),
								nickname.c_str(), svc.c_str(), email.c_str(),
								static_cast<int>(id),
								static_cast<int>(id),
								static_cast<int>(id));

							Log(LOG_COMMAND) << "[webserv] announcing new service request #" << id;
							this->PageStaff(msg);
						}
						if (id > max_id)
							max_id = id;
					}
				}
				this->last_seen_service_id = max_id;
			}
			catch (const json::exception& e)
			{
				Log(LOG_COMMAND) << "[webserv] poll: failed to parse service requests: " << e.what();
			}
		}
		else
		{
			Log(LOG_COMMAND) << "[webserv] poll: service request fetch failed: " << error;
		}
	}

	/* --- Check link requests --- */
	{
		std::string body, error;
		if (this->HttpGet("links/?status=pending&limit=50", body, error))
		{
			try
			{
				json data = json::parse(body);
				auto results = data.value("results", json::array());
				int64_t max_id = this->last_seen_link_id;

				for (const auto& r : results)
				{
					int64_t id = r.value("id", static_cast<int64_t>(0));
					if (id > this->last_seen_link_id)
					{
						if (this->first_poll_done)
						{
							std::string nickname = r.value("nickname", "");
							std::string network = r.value("network_name", "");
							std::string email = r.value("email", "");

							Anope::string msg = Anope::Format(
								"[WebServ] \002New link request #%d\002 - "
								"nick=\002%s\002 network=\002%s\002 email=%s  "
								"[\002VIEW %d links\002 | \002APPROVE %d links\002 | \002DENY %d links\002]",
								static_cast<int>(id),
								nickname.c_str(), network.c_str(), email.c_str(),
								static_cast<int>(id),
								static_cast<int>(id),
								static_cast<int>(id));

							Log(LOG_COMMAND) << "[webserv] announcing new link request #" << id;
							this->PageStaff(msg);
						}
						if (id > max_id)
							max_id = id;
					}
				}
				this->last_seen_link_id = max_id;
			}
			catch (const json::exception& e)
			{
				Log(LOG_COMMAND) << "[webserv] poll: failed to parse link requests: " << e.what();
			}
		}
		else
		{
			Log(LOG_COMMAND) << "[webserv] poll: link request fetch failed: " << error;
		}
	}

	if (!this->first_poll_done)
	{
		this->first_poll_done = true;
		Log(LOG_COMMAND) << "[webserv] poll: initial sync complete (service max_id="
		                 << this->last_seen_service_id << " link max_id=" << this->last_seen_link_id << ")";
	}
}

/* ------------------------------------------------------------------ */
/*  Module lifecycle                                                   */
/* ------------------------------------------------------------------ */

WebServCore::WebServCore(const Anope::string& modname, const Anope::string& creator)
	: Module(modname, creator, PSEUDOCLIENT | THIRD)
	, command_list(this, *this)
	, command_view(this, *this)
	, command_approve(this, *this)
	, command_deny(this, *this)
	, command_stats(this, *this)
	, command_poll(this, *this)
{
	if (!IRCD)
		throw ModuleException("IRCd protocol module not loaded");
}

WebServCore::~WebServCore()
{
	this->poll_timer = nullptr;
}

void WebServCore::OnReload(Configuration::Conf& conf)
{
	const Configuration::Block* mod = &conf.GetModule(this);

	/* Find the bot nick */
	Anope::string nick = mod->Get<const Anope::string>("client");
	if (nick.empty())
	{
		mod = &conf.GetModule("webserv");
		nick = mod->Get<const Anope::string>("client");
	}
	if (nick.empty())
	{
		mod = &conf.GetModule("webserv.so");
		nick = mod->Get<const Anope::string>("client");
	}
	if (nick.empty())
		throw ConfigException(Module::name + ": <client> must be defined");

	BotInfo* bi = BotInfo::Find(nick, true);
	if (!bi)
		throw ConfigException(Module::name + ": no bot named " + nick);

	this->WebServ = bi;

	/* API settings */
	this->api_url = mod->Get<Anope::string>("api_url", "");
	if (this->api_url.empty())
		throw ConfigException(Module::name + ": <api_url> must be defined");
	/* ensure trailing slash */
	if (this->api_url[this->api_url.length() - 1] != '/')
		this->api_url += "/";

	this->api_token = mod->Get<Anope::string>("api_token", "");
	if (this->api_token.empty())
		throw ConfigException(Module::name + ": <api_token> must be defined");

	/* Staff / privilege settings */
	this->staff_target = mod->Get<Anope::string>("staff_target", "");
	this->staff_priv = mod->Get<Anope::string>("staff_priv", "webserv/staff");

	/* Reply method */
	Anope::string rm = mod->Get<Anope::string>("reply_method", "notice");
	this->reply_with_notice = !rm.equals_ci("privmsg");

	/* Poll interval for auto-notifications (default 60s, 0 = disabled) */
	this->poll_interval = mod->Get<time_t>("poll_interval", "60");

	/* Join staff channel if configured */
	if (!this->staff_target.empty() && this->staff_target[0] == '#')
		this->WebServ->Join(this->staff_target);

	/* (Re-)create the poll timer */
	if (this->poll_timer)
	{
		delete this->poll_timer;
		this->poll_timer = nullptr;
	}
	if (this->poll_interval > 0)
	{
		this->poll_timer = new WebServPollTimer(*this, this->poll_interval);
		Log(LOG_COMMAND) << "[webserv] poll timer started (every " << this->poll_interval << "s)";
	}

	/* Run an immediate initial poll to seed last-seen IDs.
	 * The first call sets first_poll_done = true without announcing.
	 * Subsequent timer ticks will then announce genuinely new requests. */
	if (!this->first_poll_done && Me && Me->IsSynced())
	{
		Log(LOG_COMMAND) << "[webserv] running initial sync poll...";
		this->PollForNewRequests();
	}

	Log(LOG_COMMAND) << "[webserv] loaded: client=" << nick << " api_url=" << this->api_url
	                 << " staff_target=" << (this->staff_target.empty() ? "(none)" : this->staff_target)
	                 << " poll_interval=" << this->poll_interval;
}

EventReturn WebServCore::OnPreHelp(CommandSource& source, const std::vector<Anope::string>& params)
{
	if (!this->WebServ || source.service != *this->WebServ)
		return EVENT_CONTINUE;

	if (params.empty())
	{
		this->Reply(source, "WebServ — IRC service-request management.");
		this->Reply(source, " ");
		this->Reply(source, "Commands:");
		this->Reply(source, "  \002LIST\002    [service|links] [status]  — List pending requests");
		this->Reply(source, "  \002VIEW\002    <id> [links]              — View request details");
		this->Reply(source, "  \002APPROVE\002 <id> [links] [notes]      — Approve a request");
		this->Reply(source, "  \002DENY\002    <id> [links] [reason]     — Deny a request");
		this->Reply(source, "  \002STATS\002                             — Show request statistics");
		this->Reply(source, "  \002POLL\002                              — Manually check for new requests");
		this->Reply(source, " ");
		this->Reply(source, "Use \002HELP <command>\002 for more details.");
		return EVENT_STOP;
	}

	return EVENT_CONTINUE;
}
