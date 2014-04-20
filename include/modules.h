/* Modular support
 *
 * (C) 2003-2014 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 *
 */

#pragma once

#include "serialize.h"
#include "base.h"
#include "modes.h"
#include "timers.h"
#include "logger.h"
#include "extensible.h"

/** This definition is used as shorthand for the various classes
 * and functions needed to make a module loadable by the OS.
 * It defines the class factory and external AnopeInit and AnopeFini functions.
 */
#ifdef _WIN32
# define MODULE_INIT(x) \
	extern "C" DllExport Module *AnopeInit(const Anope::string &, const Anope::string &); \
	extern "C" Module *AnopeInit(const Anope::string &modname, const Anope::string &creator) \
	{ \
		return new x(modname, creator); \
	} \
	BOOLEAN WINAPI DllMain(HINSTANCE, DWORD, LPVOID) \
	{ \
		return TRUE; \
	} \
	extern "C" DllExport void AnopeFini(x *); \
	extern "C" void AnopeFini(x *m) \
	{ \
		delete m; \
	}
#else
# define MODULE_INIT(x) \
	extern "C" DllExport Module *AnopeInit(const Anope::string &modname, const Anope::string &creator) \
	{ \
		return new x(modname, creator); \
	} \
	extern "C" DllExport void AnopeFini(x *m) \
	{ \
		delete m; \
	}
#endif

/** Possible return types from events.
 */
enum EventReturn
{
	EVENT_STOP,
	EVENT_CONTINUE,
	EVENT_ALLOW
};

enum ModuleReturn
{
	MOD_ERR_OK,
	MOD_ERR_PARAMS,
	MOD_ERR_EXISTS,
	MOD_ERR_NOEXIST,
	MOD_ERR_NOLOAD,
	MOD_ERR_UNKNOWN,
	MOD_ERR_FILE_IO,
	MOD_ERR_EXCEPTION,
	MOD_ERR_VERSION
};

/* Module types, in the order in which they are unloaded. The order these are in is IMPORTANT */
enum
{
	MT_BEGIN,
	/* Module is 3rd party. All 3rd party modules should set this. Mutually exclusive to VENDOR. */
	THIRD = 1 << 0,
	/* A vendor module, which is made and shipped by Anope. Mutually exclusive to THIRD. */
	VENDOR = 1 << 1,
	/* Extra module not required for standard use. Probably requires external dependencies.
	 * This module does something extreme enough that we want it to show in the default /os modlist command
	 */
	EXTRA = 1 << 2,
	/* Module provides access to a database */
	DATABASE = 1 << 3,
	/* Module provides encryption */
	ENCRYPTION = 1 << 4,
	/* Module provides a pseudoclient */
	PSEUDOCLIENT = 1 << 5,
	/* Module provides IRCd protocol support */
	PROTOCOL = 1 << 6,
	MT_END = 1 << 7
};
typedef unsigned short ModType;

/** Returned by Module::GetVersion, used to see what version of Anope
 * a module is compiled against.
 */
class ModuleVersion
{
 private:
	int version_major;
	int version_minor;
	int version_patch;

 public:
	/** Constructor
	 * @param major The major version number
	 * @param minor The minor version number
	 * @param patch The patch version number
	 */
	ModuleVersion(int major, int minor, int patch);

	/** Get the major version of Anope this was built against
	 * @return The major version
	 */
	int GetMajor() const;

	/** Get the minor version of Anope this was built against
	 * @return The minor version
	 */
	int GetMinor() const;

	/** Get the patch version this was built against
	 * @return The patch version
	 */
	int GetPatch() const;
};

/** Every module in Anope is actually a class.
 */
class CoreExport Module : public Extensible
{
 private:
	bool permanent;
 public:
	/** The module name (e.g. os_modload)
	 */
	Anope::string name;

	/** What type this module is
	 */
	ModType type;

	/** The temporary path/filename
	 */
	Anope::string filename;

	/** Handle for this module, obtained from dlopen()
	 */
	void *handle;

	/** Time this module was created
	 */
	time_t created;

	/** Version of this module
	 */
	Anope::string version;

	/** Author of the module
	 */
	Anope::string author;

	/** Creates and initialises a new module.
	 * @param modname The module name
	 * @param loadernick The nickname of the user loading the module.
	 * @param type The module type
	 */
	Module(const Anope::string &modname, const Anope::string &loadernick, ModType type = THIRD);

	/** Destroys a module, freeing resources it has allocated.
	 */
	virtual ~Module();

	/** Toggles the permanent flag on a module. If a module is permanent,
	 * then it may not be unloaded.
	 *
	 * Naturally, this setting should be used sparingly!
	 *
	 * @param state True if this module should be permanent, false else.
	 */
	void SetPermanent(bool state);

	/** Retrieves whether or not a given module is permanent.
	 * @return true if the module is permanent, false else.
	 */
	bool GetPermanent() const;

	/** Set the modules version info.
	 * @param version the version of the module
	 */
	void SetVersion(const Anope::string &version);

	/** Set the modules author info
	 * @param author the author of the module
	 */
	void SetAuthor(const Anope::string &author);

	/** Get the version of Anope this module was
	 * compiled against
	 * @return The version
	 */
	ModuleVersion GetVersion() const;

	/** Called when Services' configuration is being (re)loaded.
	 * @param conf The config that is being built now and will replace the global Config object
	 * @throws A ConfigException to abort the config (re)loading process.
	 */
	virtual void OnReload(Configuration::Conf *conf) { }
};

/** Used to manage modules.
 */
class CoreExport ModuleManager
{
 public:
 	/** List of all modules loaded in Anope
	 */
	static std::list<Module *> Modules;

#ifdef _WIN32
	/** Clean up the module runtime directory
	 */
	static void CleanupRuntimeDirectory();
#endif

	/** Loads a given module.
	 * @param m the module to load
	 * @param u the user who loaded it, NULL for auto-load
	 * @return MOD_ERR_OK on success, anything else on fail
	 */
	static ModuleReturn LoadModule(const Anope::string &modname, User *u);

	/** Unload the given module.
	 * @param m the module to unload
	 * @param u the user who unloaded it
	 * @return MOD_ERR_OK on success, anything else on fail
	 */
	static ModuleReturn UnloadModule(Module *m, User * u);

	/** Find a module
	 * @param name The module name
	 * @return The module
	 */
	static Module *FindModule(const Anope::string &name);

	/** Find the first module of a certain type
	 * @param type The module type
	 * @return The module
	 */
	static Module *FindFirstOf(ModType type);

	/** Checks whether this version of Anope is at least major.minor.patch.build
	 * Throws a ModuleException if not
	 * @param major The major version
	 * @param minor The minor vesion
	 * @param patch The patch version
	 */
	static void RequireVersion(int major, int minor, int patch);

	/** Unloading all modules except the protocol module.
	 */
	static void UnloadAll();

 private:
	/** Call the module_delete function to safely delete the module
	 * @param m the module to delete
	 * @return MOD_ERR_OK on success, anything else on fail
	 */
	static ModuleReturn DeleteModule(Module *m);
};

