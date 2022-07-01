#include "module.h"
#include "modules/set_misc.h"

class ModuleRPLChanURL : public Module
{
 public:
	ModuleRPLChanURL (const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
	}

	void OnJoinChannel(User *u, Channel *c) anope_override
	{
		if (!u->server->IsSynced())
			return;

		ChannelInfo *ci = ChannelInfo::Find(c->name);
		if (!ci)
			return;

		MiscData* data = ci->GetExt<MiscData>("cs_set_misc:URL");
		if (!data)
			return;

		IRCD->SendNumeric(328, u->nick, "%s :%s", c->name.c_str(), data->data.c_str());
	}
};

MODULE_INIT(ModuleRPLChanURL )
