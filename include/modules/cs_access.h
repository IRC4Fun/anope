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
	struct CoreExport LevelChange : Events
	{
		/** Called when a level for a channel is changed
		 * @param source The source of the command
		 * @param ci The channel the level was changed on
		 * @param priv The privilege changed
		 * @param what The new level
		 */
		virtual void OnLevelChange(CommandSource &source, ChannelInfo *ci, const Anope::string &priv, int16_t what) anope_abstract;
	};
}
