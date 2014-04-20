/*
 *
 * (C) 2014 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 */

namespace Event
{
	struct BotFantasy : Events
	{
		/** Called on fantasy command
		 * @param source The source of the command
		 * @param c The command
		 * @param ci The channel it's being used in
		 * @param params The params
		 * @return EVENT_STOP to halt processing and not run the command, EVENT_ALLOW to allow the command to be executed
		 */
		virtual EventReturn OnBotFantasy(CommandSource &source, Command *c, ChannelInfo *ci, const std::vector<Anope::string> &params) anope_abstract;
	};

	struct CoreExport BotNoFantasyAccess : Events
	{
		/** Called on fantasy command without access
		 * @param source The source of the command
		 * @param c The command
		 * @param ci The channel it's being used in
		 * @param params The params
		 * @return EVENT_STOP to halt processing and not run the command, EVENT_ALLOW to allow the command to be executed
		 */
		virtual EventReturn OnBotNoFantasyAccess(CommandSource &source, Command *c, ChannelInfo *ci, const std::vector<Anope::string> &params) anope_abstract;
	};
}
