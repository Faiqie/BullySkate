/* DERPY'S SCRIPT LOADER: TEXT
	
	some big strings are put here for neatness
	general strings that are used often are also here
	
*/

/* COMMANDS */

// BASIC
#define TEXT_HELP_QUIT "Usage: quit\nSafely terminate the server as soon as possible."
#define TEXT_HELP_HELP "Usage: help [command]\nLists all commands, or get help with a specific command."
#define TEXT_HELP_CLEAR "Usage: clear\nClears the console."

// LOADER
#define TEXT_HELP_START "Usage: start <collection>\nStart / restart a specific script collection, or use \"*\" to start / restart all collections."
#define TEXT_HELP_STOP "Usage: stop <collection>\nStop a specific script collection, or use \"*\" to stop all running collections."
#define TEXT_HELP_RESTART "Usage: restart <collection>\nStart / restart a specific script collection, or use \"*\" to restart all running collections."
#define TEXT_HELP_LIST "Usage: list [status]\nLists all script collections, optionally using a status to filter which ones to list."
#define TEXT_HELP_CHECK "Usage: check <collection>\nCheck the current status of a script collection, or use \"*\" to check all collections."

// NETWORK
#define TEXT_HELP_CONNECT "Usage: connect <ip>[:port]\nConnect to a derpy_script_server."
#define TEXT_HELP_DISCONNECT "Usage: disconnect\nDisconnect from the connected derpy_script_server."
#define TEXT_HELP_RELOAD_CONFIG "Usage: reload_config\nReload server config. Most settings only matter at startup, but you can still update the blacklist/whitelist."

/* FAILURES */

// GENERAL
#define TEXT_FAIL_ALLOCATE "failed to allocate memory for %s"
#define TEXT_FAIL_UNKNOWN "an unknown error occurred"