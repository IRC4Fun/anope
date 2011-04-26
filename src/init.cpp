/* Initalization and related routines.
 *
 * (C) 2003-2011 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "services.h"
#include "modules.h"

Uplink *uplink_server;

void introduce_user(const Anope::string &user)
{
	/* Watch out for infinite loops... */
	time_t now = Anope::CurTime;
	static time_t lasttime = now - 4;
	if (lasttime >= now - 3)
		throw FatalException("introduce_user loop detected");
	lasttime = now;

	if (!user.empty())
	{
		User *u = finduser(user);
		if (u)
		{
			ircdproto->SendClientIntroduction(u, ircd->pseudoclient_mode);

			BotInfo *bi = findbot(u->nick);
			if (bi)
			{
				XLine x(bi->nick, "Reserved for services");
				ircdproto->SendSQLine(NULL, &x);

				for (UChannelList::const_iterator cit = bi->chans.begin(), cit_end = bi->chans.end(); cit != cit_end; ++cit)
					ircdproto->SendJoin(bi, (*cit)->chan, &Config->BotModeList);
			}
		}

		return;
	}

	ircdproto->SendBOB();
	
	for (unsigned i = 0; i < Me->GetLinks().size(); ++i)
	{
		Server *s = Me->GetLinks()[i];

		if (s->HasFlag(SERVER_JUPED))
		{
			ircdproto->SendServer(s);
		}
	}

	/* We make the bots go online */
	for (Anope::insensitive_map<User *>::iterator it = UserListByNick.begin(), it_end = UserListByNick.end(); it != it_end; ++it)
	{
		User *u = it->second;

		ircdproto->SendClientIntroduction(u, ircd->pseudoclient_mode);

		BotInfo *bi = findbot(u->nick);
		if (bi)
		{
			XLine x(bi->nick, "Reserved for services");
			ircdproto->SendSQLine(NULL, &x);

			for (UChannelList::const_iterator cit = bi->chans.begin(), cit_end = bi->chans.end(); cit != cit_end; ++cit)
				ircdproto->SendJoin(bi, (*cit)->chan, &Config->BotModeList);
		}
	}

	/* Load MLock from the database now that we know what modes exist */
	for (registered_channel_map::iterator it = RegisteredChannelList.begin(), it_end = RegisteredChannelList.end(); it != it_end; ++it)
		it->second->LoadMLock();
}

/*************************************************************************/

/* Set GID if necessary.  Return 0 if successful (or if RUNGROUP not
 * defined), else print an error message to logfile and return -1.
 */

static int set_group()
{
#if defined(RUNGROUP) && defined(HAVE_SETGRENT)
	struct group *gr;

	setgrent();
	while ((gr = getgrent()))
	{
		if (!strcmp(gr->gr_name, RUNGROUP))
			break;
	}
	endgrent();
	if (gr)
	{
		setgid(gr->gr_gid);
	}
	else
	{
		Log() << "Unknown run group '" << RUNGROUP << "'";
		return -1;
	}
#endif
	return 0;
}

/*************************************************************************/

/* Vector of pairs of command line arguments and their params */
static std::vector<std::pair<Anope::string, Anope::string> > CommandLineArguments;

/** Called on startup to organize our starting arguments in a better way
 * and check for errors
 * @param ac number of args
 * @param av args
 */
static void ParseCommandLineArguments(int ac, char **av)
{
	for (int i = 1; i < ac; ++i)
	{
		Anope::string option = av[i];
		Anope::string param;
		while (!option.empty() && option[0] == '-')
			option.erase(option.begin());
		size_t t = option.find('=');
		if (t != Anope::string::npos)
		{
			param = option.substr(t + 1);
			option.erase(t);
		}

		if (option.empty())
			continue;

		CommandLineArguments.push_back(std::make_pair(option, param));
	}
}

/** Check if an argument was given on startup
 * @param name The argument name
 * @param shortname A shorter name, eg --debug and -d
 * @return true if name/shortname was found, false if not
 */
bool GetCommandLineArgument(const Anope::string &name, char shortname)
{
	Anope::string Unused;
	return GetCommandLineArgument(name, shortname, Unused);
}

/** Check if an argument was given on startup and its parameter
 * @param name The argument name
 * @param shortname A shorter name, eg --debug and -d
 * @param param A string to put the param, if any, of the argument
 * @return true if name/shortname was found, false if not
 */
bool GetCommandLineArgument(const Anope::string &name, char shortname, Anope::string &param)
{
	param.clear();

	for (std::vector<std::pair<Anope::string, Anope::string> >::iterator it = CommandLineArguments.begin(), it_end = CommandLineArguments.end(); it != it_end; ++it)
	{
		if (it->first.equals_ci(name) || it->first[0] == shortname)
		{
			param = it->second;
			return true;
		}
	}

	return false;
}

/*************************************************************************/

/* Remove our PID file.  Done at exit. */

static void remove_pidfile()
{
	remove(Config->PIDFilename.c_str());
}

/*************************************************************************/

/* Create our PID file and write the PID to it. */

static void write_pidfile()
{
	FILE *pidfile;

	pidfile = fopen(Config->PIDFilename.c_str(), "w");
	if (pidfile)
	{
#ifdef _WIN32
		fprintf(pidfile, "%d\n", static_cast<int>(GetCurrentProcessId()));
#else
		fprintf(pidfile, "%d\n", static_cast<int>(getpid()));
#endif
		fclose(pidfile);
		atexit(remove_pidfile);
	}
	else
		throw FatalException("Can not write to PID file " + Config->PIDFilename);
}

/*************************************************************************/

void Init(int ac, char **av)
{
	int started_from_term = isatty(0) && isatty(1) && isatty(2);

	/* Set file creation mask and group ID. */
#if defined(DEFUMASK) && HAVE_UMASK
	umask(DEFUMASK);
#endif
	if (set_group() < 0)
		throw FatalException("set_group() fail");

	/* Parse command line arguments */
	ParseCommandLineArguments(ac, av);

	if (GetCommandLineArgument("version", 'v'))
	{
		Log(LOG_TERMINAL) << "Anope-" << Anope::Version() << " -- " << Anope::VersionBuildString();
		throw FatalException();
	}

	if (GetCommandLineArgument("help", 'h'))
	{
		Log(LOG_TERMINAL) << "Anope-" << Anope::Version() << " -- " << Anope::VersionBuildString();
		Log(LOG_TERMINAL) << "Anope IRC Services (http://www.anope.org)";
		Log(LOG_TERMINAL) << "Usage ./" << services_bin << " [options] ...";
		Log(LOG_TERMINAL) << "-c, --config=filename.conf";
		Log(LOG_TERMINAL) << "-d, --debug[=level]";
		Log(LOG_TERMINAL) << "    --dir=services_directory";
		Log(LOG_TERMINAL) << "-h, --help";
		Log(LOG_TERMINAL) << "-e, --noexpire";
		Log(LOG_TERMINAL) << "-n, --nofork";
		Log(LOG_TERMINAL) << "    --nothird";
		Log(LOG_TERMINAL) << "    --protocoldebug";
		Log(LOG_TERMINAL) << "-r, --readonly";
		Log(LOG_TERMINAL) << "-s, --support";
		Log(LOG_TERMINAL) << "-v, --version";
		Log(LOG_TERMINAL) << "";
		Log(LOG_TERMINAL) << "Further support is available from http://www.anope.org";
		Log(LOG_TERMINAL) << "Or visit us on IRC at irc.anope.org #anope";
		throw FatalException();
	}

	if (GetCommandLineArgument("nofork", 'n'))
		nofork = true;

	if (GetCommandLineArgument("support", 's'))
	{
		nofork = nothird = true;
		++debug;
	}

	if (GetCommandLineArgument("readonly", 'r'))
		readonly = true;

	if (GetCommandLineArgument("nothird"))
		nothird = true;

	if (GetCommandLineArgument("noexpire", 'e'))
		noexpire = true;

	if (GetCommandLineArgument("protocoldebug"))
		protocoldebug = true;

	Anope::string Arg;
	if (GetCommandLineArgument("debug", 'd', Arg))
	{
		if (!Arg.empty())
		{
			int level = Arg.is_number_only() ? convertTo<int>(Arg) : -1;
			if (level > 0)
				debug = level;
			else
				throw FatalException("Invalid option given to --debug");
		}
		else
			++debug;
	}

	if (GetCommandLineArgument("config", 'c', Arg))
	{
		if (Arg.empty())
			throw FatalException("The --config option requires a file name");
		services_conf = ConfigurationFile(Arg, false);
	}

	if (GetCommandLineArgument("dir", 0, Arg))
	{
		if (Arg.empty())
			throw FatalException("The --dir option requires a directory name");
		services_dir = Arg;
	}

	/* Chdir to Services data directory. */
	if (chdir(services_dir.c_str()) < 0)
	{
		throw FatalException("Unable to chdir to " + services_dir + ": " + Anope::LastError());
	}

	init_core_messages();

	Log(LOG_TERMINAL) << "Anope " << Anope::Version() << ", " << Anope::VersionBuildString();
#ifdef _WIN32
	Log(LOG_TERMINAL) << "Using configuration file " << services_dir << "\\" << services_conf.GetName();
#else
	Log(LOG_TERMINAL) << "Using configuration file " << services_dir << "/" << services_conf.GetName();
#endif

	/* Read configuration file; exit if there are problems. */
	try
	{
		Config = new ServerConfig();
	}
	catch (const ConfigException &ex)
	{
		Log(LOG_TERMINAL) << ex.GetReason();
		Log(LOG_TERMINAL) << "*** Support resources: Read through the services.conf self-contained";
		Log(LOG_TERMINAL) << "*** documentation. Read the documentation files found in the 'docs'";
		Log(LOG_TERMINAL) << "*** folder. Visit our portal located at http://www.anope.org/. Join";
		Log(LOG_TERMINAL) << "*** our support channel on /server irc.anope.org channel #anope.";
		throw FatalException("Configuration file failed to validate");
	}

	/* Initialize the socket engine */
	SocketEngine::Init();

	/* Add IRCD Protocol Module; exit if there are errors */
	if (protocol_module_init())
		throw FatalException("Unable to load protocol module");

	/* Create me */
	Me = new Server(NULL, Config->ServerName, 0, Config->ServerDesc, Config->Numeric);

	/* Add Encryption Modules */
	ModuleManager::LoadModuleList(Config->EncModuleList);

	/* Add Database Modules */
	ModuleManager::LoadModuleList(Config->DBModuleList);

	DNSEngine = new DNSManager();

#ifndef _WIN32
	if (!nofork)
	{
		int i;
		if ((i = fork()) < 0)
			throw FatalException("Unable to fork");
		else if (i != 0)
		{
			Log(LOG_TERMINAL) << "PID " << i;
			exit(0);
		}

		if (started_from_term)
		{
			close(0);
			close(1);
			close(2);
		}
		if (setpgid(0, 0) < 0)
			throw FatalException("Unable to setpgid()");
	}
#else
	if (!SupportedWindowsVersion())
		throw FatalException(GetWindowsVersion() + " is not a supported version of Windows");
	if (!nofork)
	{
		Log(LOG_TERMINAL) << "PID " << GetCurrentProcessId();
		Log() << "Launching Anope into the background";
		FreeConsole();
	}
#endif

	/* Write our PID to the PID file. */
	write_pidfile();

	/* Announce ourselves to the logfile. */
	Log() << "Anope " << Anope::Version() << " (ircd protocol: " << ircd->name << ") starting up" << (debug || readonly ? " (options:" : "") << (debug ? " debug" : "") << (readonly ? " readonly" : "") << (debug || readonly ? ")" : "");
	start_time = Anope::CurTime;

	/* Set signal handlers.  Catch certain signals to let us do things or
	 * panic as necessary, and ignore all others.
	 */
#ifndef _WIN32
	signal(SIGHUP, sighandler);
#endif
	signal(SIGTERM, sighandler);
	signal(SIGINT, sighandler);

	/* Initialize multi-language support */
	Log(LOG_DEBUG) << "Loading Languages...";
	InitLanguages();

	/* load any custom modules */
	if (!nothird)
		ModuleManager::LoadModuleList(Config->ModulesAutoLoad);

	/* Initialize random number generator */
	rand_init();
	add_entropy_userkeys();

	/* Load up databases */
	Log() << "Loading databases...";
	EventReturn MOD_RESULT;
	FOREACH_RESULT(I_OnLoadDatabase, OnLoadDatabase());
	Log() << "Databases loaded";

	FOREACH_MOD(I_OnPostLoadDatabases, OnPostLoadDatabases());
}

/*************************************************************************/
