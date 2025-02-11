#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "imapfilter.h"
#include "pathnames.h"


extern options opts;
extern struct sessionhead sessions;

static lua_State *lua;		/* Lua interpreter state. */


static int traceback_handler(lua_State *lua);
void init_options(void);
void interactive_mode(void);


/*
 * Start the Lua interpreter, export IMAP core and system functions, load the
 * Lua interface functions, load and execute imapfilter's configuration file.
 */
void
start_lua()
{

	lua = luaL_newstate();

	luaL_openlibs(lua);

	luaopen_ifcore(lua);
	luaopen_ifsys(lua);
	luaopen_ifre(lua);

	lua_settop(lua, 0);

	init_options();

	if (luaL_loadfile(lua, PATHNAME_COMMON) ||
	    lua_pcall(lua, 0, LUA_MULTRET, 0))
		fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));

	if (luaL_loadfile(lua, PATHNAME_SET) ||
	    lua_pcall(lua, 0, LUA_MULTRET, 0))
		fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));

	if (luaL_loadfile(lua, PATHNAME_REGEX) ||
	    lua_pcall(lua, 0, LUA_MULTRET, 0))
		fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));

	if (luaL_loadfile(lua, PATHNAME_ACCOUNT) ||
	    lua_pcall(lua, 0, LUA_MULTRET, 0))
		fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));

	if (luaL_loadfile(lua, PATHNAME_MAILBOX) ||
	    lua_pcall(lua, 0, LUA_MULTRET, 0))
		fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));

	if (luaL_loadfile(lua, PATHNAME_MESSAGE) ||
	    lua_pcall(lua, 0, LUA_MULTRET, 0))
		fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));

	if (luaL_loadfile(lua, PATHNAME_OPTIONS) ||
	    lua_pcall(lua, 0, LUA_MULTRET, 0))
		fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));

	if (luaL_loadfile(lua, PATHNAME_AUXILIARY) ||
	    lua_pcall(lua, 0, LUA_MULTRET, 0))
		fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));

	if (opts.oneline != NULL) {
		if (luaL_loadbuffer(lua, opts.oneline, strlen(opts.oneline),
		    "=<command line>") || lua_pcall(lua, 0, LUA_MULTRET, 0))
			fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));
	} else {
		if (luaL_loadfile(lua, strcmp(opts.config, "-") == 0 ? NULL :
		    opts.config))
			fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));
		lua_pushcfunction(lua, traceback_handler);
		lua_insert(lua, 1);
		if (lua_pcall(lua, 0, LUA_MULTRET, -2))
			fatal(ERROR_CONFIG, "%s\n", lua_tostring(lua, -1));
	}

	if (opts.interactive)
		interactive_mode();
}


/*
 * Add a stack traceback to the error message.
 */
static int
traceback_handler(lua_State *lua)
{

#if LUA_VERSION_NUM < 502
	lua_getfield(lua, LUA_GLOBALSINDEX, "debug");
	lua_getfield(lua, -1, "traceback");
	lua_pushvalue(lua, 1);
	lua_pushinteger(lua, 2);
	lua_call(lua, 2, 1);
#else
	luaL_traceback(lua, lua, lua_tostring(lua, 1), 0);
#endif
	return 1;
}


/*
 * Stop the Lua interpreter.
 */
void
stop_lua(void)
{

	lua_close(lua);
}


/*
 * Set default values to program's options.
 */
void
init_options(void)
{

	lua_newtable(lua);

	set_table_boolean("certificates", 1);
	set_table_boolean("create", 0);
	set_table_boolean("expunge", 1);
	set_table_boolean("hostnames", 1);
	set_table_number("keepalive", 29);
	set_table_boolean("namespace", 1);
	set_table_boolean("persist", 0);
	set_table_string("recover", "all");
	set_table_boolean("reenter", 1);
	set_table_boolean("starttls", 1);
	set_table_boolean("subscribe", 0);
	set_table_number("timeout", 60);
	set_table_boolean("wakeonany", 0);

	set_table_boolean("dryrun", opts.dryrun);

	lua_setglobal(lua, "options");
}


/*
 * Interactive mode.
 */
void
interactive_mode(void)
{
	char buf[LINE_MAX];
	
	for (;;) {
		printf("> ");
		fflush(stdout);

		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			printf("\n");
			break;
		}

		if (luaL_loadbuffer(lua, buf, strlen(buf), "=<line>") ||
		    lua_pcall(lua, 0, LUA_MULTRET, 0)) {
			error("%s\n", lua_tostring(lua, -1));
			lua_pop(lua, 1);
		}
	}
}


/*
 * Get from the configuration file the value of a boolean option variable.
 */
int
get_option_boolean(const char *opt)
{
	int b;

	lua_getglobal(lua, "options");
	lua_getfield(lua, -1, opt);
	b = lua_toboolean(lua, -1);
	lua_pop(lua, 2);

	return b;
}


/*
 * Get from the configuration file the value of a number option variable.
 */
lua_Number
get_option_number(const char *opt)
{
	lua_Number n;

	lua_getglobal(lua, "options");
	lua_getfield(lua, -1, opt);
	n = lua_tonumber(lua, -1);
	lua_pop(lua, 2);

	return n;
}


/*
 * Get from the configuration file the value of a string option variable.
 */
const char *
get_option_string(const char *opt)
{
	const char *s;

	lua_getglobal(lua, "options");
	lua_getfield(lua, -1, opt);
	s = lua_tostring(lua, -1);
	lua_pop(lua, 2);

	if (!s)
		return "";

	return s;
}


/*
 * Set a table's element value to the specified boolean.
 */
int
set_table_boolean(const char *key, int value)
{

	lua_pushstring(lua, key);
	lua_pushboolean(lua, value);
	lua_settable(lua, -3);

	return 0;
}


/*
 * Set a table's element value to the specified number.
 */
int
set_table_number(const char *key, lua_Number value)
{

	lua_pushstring(lua, key);
	lua_pushnumber(lua, value);
	lua_settable(lua, -3);

	return 0;
}

/*
 * Set a table's element value to the specified string.
 */
int
set_table_string(const char *key, const char *value)
{

	lua_pushstring(lua, key);
	lua_pushstring(lua, value);
	lua_settable(lua, -3);

	return 0;
}


