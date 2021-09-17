/*
 * Copyright 2010-2021, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <assert.h>
#include <stdbool.h>
#include <math.h> /* modf, isfinite */
#include <lua.h>
#include <lauxlib.h>

#include "lua/serializer.h"

#include "trigger.h"
#include "lib/core/decimal.h" /* decimal_t */
#include "lib/core/mp_extension_types.h"
#include "lua/error.h"

#include "trivia/util.h"
#include "diag.h"
#include "serializer_opts.h"
#include "lua/utils.h"

int luaL_map_metatable_ref = LUA_REFNIL;
int luaL_array_metatable_ref = LUA_REFNIL;
extern uint32_t CTID_UUID;
extern uint32_t CTID_DECIMAL;

/* {{{ luaL_serializer manipulations */

#define OPTION(type, name, defvalue) { #name, \
	offsetof(struct luaL_serializer, name), type, defvalue}
/**
 * Configuration options for serializers
 * @sa struct luaL_serializer
 */
static struct {
	const char *name;
	size_t offset; /* offset in structure */
	int type;
	int defvalue;
} OPTIONS[] = {
	OPTION(LUA_TBOOLEAN, encode_sparse_convert, 1),
	OPTION(LUA_TNUMBER,  encode_sparse_ratio, 2),
	OPTION(LUA_TNUMBER,  encode_sparse_safe, 10),
	OPTION(LUA_TNUMBER,  encode_max_depth, 128),
	OPTION(LUA_TBOOLEAN, encode_deep_as_nil, 0),
	OPTION(LUA_TBOOLEAN, encode_invalid_numbers, 1),
	OPTION(LUA_TNUMBER,  encode_number_precision, 14),
	OPTION(LUA_TBOOLEAN, encode_load_metatables, 1),
	OPTION(LUA_TBOOLEAN, encode_use_tostring, 0),
	OPTION(LUA_TBOOLEAN, encode_invalid_as_nil, 0),
	OPTION(LUA_TBOOLEAN, decode_invalid_numbers, 1),
	OPTION(LUA_TBOOLEAN, decode_save_metatables, 1),
	OPTION(LUA_TNUMBER,  decode_max_depth, 128),
	{ NULL, 0, 0, 0},
};

void
luaL_serializer_create(struct luaL_serializer *cfg)
{
	rlist_create(&cfg->on_update);
	for (int i = 0; OPTIONS[i].name != NULL; i++) {
		int *pval = (int *) ((char *) cfg + OPTIONS[i].offset);
		*pval = OPTIONS[i].defvalue;
	}
}

void
luaL_serializer_copy_options(struct luaL_serializer *dst,
			     const struct luaL_serializer *src)
{
	memcpy(dst, src, offsetof(struct luaL_serializer, end_of_options));
}

/**
 * Configure one field in @a cfg. Value of the field is kept on
 * Lua stack after this function, and should be popped manually.
 * @param L Lua stack.
 * @param i Index of option in OPTIONS[].
 * @param cfg Serializer to inherit configuration.
 * @retval Pointer to the value of option.
 * @retval NULL if option is not in the table.
 */
static int *
luaL_serializer_parse_option(struct lua_State *L, int i,
			     struct luaL_serializer *cfg)
{
	lua_getfield(L, 2, OPTIONS[i].name);
	if (lua_isnil(L, -1))
		return NULL;
	/*
	 * Update struct luaL_serializer using pointer to a
	 * configuration value (all values must be `int` for that).
	*/
	int *pval = (int *) ((char *) cfg + OPTIONS[i].offset);
	switch (OPTIONS[i].type) {
	case LUA_TBOOLEAN:
		*pval = lua_toboolean(L, -1);
		break;
	case LUA_TNUMBER:
		*pval = lua_tointeger(L, -1);
		break;
	default:
		unreachable();
	}
	return pval;
}

void
luaL_serializer_parse_options(struct lua_State *L,
			      struct luaL_serializer *cfg)
{
	for (int i = 0; OPTIONS[i].name != NULL; ++i) {
		luaL_serializer_parse_option(L, i, cfg);
		lua_pop(L, 1);
	}
}

/**
 * @brief serializer.cfg{} Lua binding for serializers.
 * serializer.cfg is a table that contains current configuration values from
 * luaL_serializer structure. serializer.cfg has overriden __call() method
 * to change configuration keys in internal userdata (like box.cfg{}).
 * Please note that direct change in serializer.cfg.key will not affect
 * internal state of userdata. Changes via cfg() are reflected in
 * both Lua cfg table, and C serializer structure.
 * @param L lua stack
 * @return 0
 */
static int
luaL_serializer_cfg(struct lua_State *L)
{
	/* Serializer.cfg */
	luaL_checktype(L, 1, LUA_TTABLE);
	/* Updated parameters. */
	luaL_checktype(L, 2, LUA_TTABLE);
	struct luaL_serializer *cfg = luaL_checkserializer(L);
	for (int i = 0; OPTIONS[i].name != NULL; ++i) {
		if (luaL_serializer_parse_option(L, i, cfg) == NULL)
			lua_pop(L, 1);
		else
			lua_setfield(L, 1, OPTIONS[i].name);
	}
	trigger_run(&cfg->on_update, cfg);
	return 0;
}

struct luaL_serializer *
luaL_newserializer(struct lua_State *L, const char *modname,
		   const luaL_Reg *reg)
{
	luaL_checkstack(L, 1, "too many upvalues");

	/* Create new module */
	lua_newtable(L);

	/* Create new configuration */
	struct luaL_serializer *serializer = (struct luaL_serializer *)
			lua_newuserdata(L, sizeof(*serializer));
	luaL_getmetatable(L, LUAL_SERIALIZER);
	lua_setmetatable(L, -2);
	luaL_serializer_create(serializer);

	for (; reg->name != NULL; reg++) {
		/* push luaL_serializer as upvalue */
		lua_pushvalue(L, -1);
		/* register method */
		lua_pushcclosure(L, reg->func, 1);
		lua_setfield(L, -3, reg->name);
	}

	/* Add cfg{} */
	lua_newtable(L); /* cfg */
	lua_newtable(L); /* metatable */
	lua_pushvalue(L, -3); /* luaL_serializer */
	lua_pushcclosure(L, luaL_serializer_cfg, 1);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);
	/* Save configuration values to serializer.cfg */
	for (int i = 0; OPTIONS[i].name != NULL; i++) {
		int *pval = (int *) ((char *) serializer + OPTIONS[i].offset);
		switch (OPTIONS[i].type) {
		case LUA_TBOOLEAN:
			lua_pushboolean(L, *pval);
			break;
		case LUA_TNUMBER:
			lua_pushinteger(L, *pval);
			break;
		default:
			unreachable();
		}
		lua_setfield(L, -2, OPTIONS[i].name);
	}
	lua_setfield(L, -3, "cfg");

	lua_pop(L, 1);  /* remove upvalues */

	luaL_pushnull(L);
	lua_setfield(L, -2, "NULL");
	lua_rawgeti(L, LUA_REGISTRYINDEX, luaL_array_metatable_ref);
	lua_setfield(L, -2, "array_mt");
	lua_rawgeti(L, LUA_REGISTRYINDEX, luaL_map_metatable_ref);
	lua_setfield(L, -2, "map_mt");

	if (modname != NULL) {
		/* Register module */
		lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
		lua_pushstring(L, modname); /* add alias */
		lua_pushvalue(L, -3);
		lua_settable(L, -3);
		lua_pop(L, 1); /* _LOADED */
	}

	return serializer;
}

/* }}} luaL_serializer manipulations */

/* {{{ Fill luaL_field */

static int
lua_gettable_wrapper(lua_State *L)
{
	lua_gettable(L, -2);
	return 1;
}

static void
lua_field_inspect_ucdata(struct lua_State *L, struct luaL_serializer *cfg,
			int cache_index, int idx, struct luaL_field *field)
{
	if (!cfg->encode_load_metatables)
		return;

	/*
	 * Try to call LUAL_SERIALIZE method on udata/cdata
	 * LuaJIT specific: lua_getfield/lua_gettable raises exception on
	 * cdata if field doesn't exist.
	 */
	int top = lua_gettop(L);
	lua_pushcfunction(L, lua_gettable_wrapper);
	lua_pushvalue(L, idx);
	lua_pushliteral(L, LUAL_SERIALIZE);
	if (lua_pcall(L, 2, 1, 0) == 0  && !lua_isnil(L, -1)) {
		if (!lua_isfunction(L, -1))
			luaL_error(L, "invalid " LUAL_SERIALIZE  " value");
		/* copy object itself */
		lua_pushvalue(L, idx);
		lua_pcall(L, 1, 1, 0);
		/* replace obj with the unpacked value */
		lua_replace(L, idx);
		if (luaL_tofield(L, cfg, NULL, cache_index, idx, field) < 0)
			luaT_error(L);
	} /* else ignore lua_gettable exceptions */
	lua_settop(L, top); /* remove temporary objects */
}

static const char *assigned_nil = "__assigned_nil";

int
luaL_pre_serialize(struct lua_State *L, int cache_index, int idx)
{
	if (lua_type(L, idx) != LUA_TTABLE)
		return 0;

	if (luaL_getmetafield(L, idx, LUAL_SERIALIZE) != 0) {
		if (lua_isfunction(L, -1)) {
			lua_pushvalue(L, idx);
			lua_rawget(L, cache_index);
			if (!lua_isnil(L, -1)) {
				lua_replace(L, idx);
				lua_pop(L, 1);
				return 0;
			}
			lua_pop(L, 1);

			/*
			 * Push copy of processed table on top of
			 * stack to use it as an argument of
			 * serializing function.
			 */
			lua_pushvalue(L, idx);
			if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
				diag_set(LuajitError, lua_tostring(L, -1));
				return -1;
			}

			if (lua_isnil(L, -1)) {
				/*
				 * Process the special case, when
				 * "__serialize" function returns
				 * nil.
				 */
				lua_pushstring(L, assigned_nil);
				lua_replace(L, -2);
			}

			/* Create an entry in cache table. */
			lua_pushvalue(L, idx);
			lua_pushvalue(L, -2);
			lua_rawset(L, cache_index);


			lua_replace(L, idx);
			if (lua_type(L, -1) != LUA_TTABLE)
				return 0;
		} else {
			lua_pop(L, 1);
			return 0;
		}
	}

	/* Process other values and keys in the table. */
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		if (luaL_pre_serialize(L, cache_index, lua_gettop(L)) != 0)
			return -1;
		lua_pop(L, 1);
		lua_pushvalue(L, -1);
		if (luaL_pre_serialize(L, cache_index, lua_gettop(L)) != 0)
			return -1;
		lua_pop(L, 1);
	}
	return 0;
}

void
luaL_find_references(struct lua_State *L, int anchortable_index,
		     int cache_index)
{
	int newval;

	if (lua_type(L, -1) != LUA_TTABLE)
		return;

	/*
	 * Check if pocessed table has serialization result. If it
	 * is, replace the table with the result.
	 */
	lua_pushvalue(L, -1);
	lua_rawget(L, cache_index);
	if (lua_type(L, -1) != LUA_TTABLE) {
		bool is_nil = lua_isnil(L, -1);
		lua_pop(L, 1);
		if (!is_nil)
			return;
	} else {
		lua_replace(L, -2);
	}
	lua_pushvalue(L, -1);

	lua_rawget(L, anchortable_index);
	if (lua_isnil(L, -1))
		newval = 0;
	else if (!lua_toboolean(L, -1))
		newval = 1;
	else
		newval = -1;
	lua_pop(L, 1);

	if (newval != -1) {
		lua_pushvalue(L, -1);
		lua_pushboolean(L, newval);
		lua_rawset(L, anchortable_index);
	}

	if (newval != 0)
		return;

	/* Process other values and keys in the table. */
	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		luaL_find_references(L, anchortable_index, cache_index);
		lua_pop(L, 1);
		luaL_find_references(L, anchortable_index, cache_index);
	}
}

int
luaL_get_anchor(struct lua_State *L, int anchortable_index,
		unsigned int *anchor_number, const char **anchor)
{
	lua_pushvalue(L, -1);
	lua_rawget(L, anchortable_index);
	if (lua_toboolean(L, -1) == 0) {
		/* This element is not referenced. */
		lua_pop(L, 1);
		*anchor = NULL;
		return GET_ANCHOR_NOT_REFERNCED;
	}

	if (lua_isboolean(L, -1) != 0) {
		/*
		 * This element is referenced more than once, but
		 * has not been named.
		 */
		char buf[32];
		snprintf(buf, sizeof(buf), "%u", (*anchor_number)++);
		lua_pop(L, 1);
		/* Generate a string anchor and push to table. */
		lua_pushvalue(L, -1);
		lua_pushstring(L, buf);
		*anchor = lua_tostring(L, -1);
		lua_rawset(L, anchortable_index);
		return GET_ANCHOR_NOT_NAMED;
	} else {
		/* This is an aliased element. */
		*anchor = lua_tostring(L, -1);
		assert(anchor != NULL);
		lua_pop(L, 1);
		return GET_ANCHOR_NAMED;
	}
	return GET_ANCHOR_NOT_REFERNCED;
}

/**
 * Call __serialize method of a table object by index
 * if the former exists.
 *
 * If __serialize does not exist then function does nothing
 * and the function returns 1;
 *
 * If __serialize exists, is a function (which doesn't
 * raise any error) then a result of serialization
 * replaces old value by the index and the function returns 0;
 *
 * If the serialization is a hint string (like 'array' or 'map'),
 * then field->type, field->size and field->compact
 * are set if necessary and the function returns 0;
 *
 * Otherwise it is an error, set diag and the funciton returns -1;
 *
 * Return values:
 * -1 - error occurs, diag is set, the top of guest stack
 *      is undefined.
 *  0 - __serialize field is available in the metatable,
 *      the result value is put in the origin slot,
 *      encoding is finished.
 *  1 - __serialize field is not available in the metatable,
 *      proceed with default table encoding or available and
 *      result (table) is taken from cache table.
 */
static int
lua_field_try_serialize(struct lua_State *L, struct luaL_serializer *cfg,
			int cache_index, int idx, struct luaL_field *field)
{
	if (luaL_getmetafield(L, idx, LUAL_SERIALIZE) == 0)
		return 1;
	if (lua_isfunction(L, -1)) {
		if (cache_index != 0) {
			/*
			 * Pop the __serialize function for the
			 * current node. It was already called in
			 * find_references_and_serialize().
			 */
			lua_pop(L, 1);
			/*
			 * Get the result of serialization from
			 * the cache.
			 */
			lua_pushvalue(L, idx);
			lua_rawget(L, cache_index);
			assert(lua_isnil(L, -1) == 0);

			if (lua_type(L, -1) == LUA_TSTRING) {
				/*
				 * Process the special case, when
				 * "__serialize" function returns
				 * nil.
				 */
				const char *val = lua_tolstring(L , -1, NULL);
				if (strcmp(val, assigned_nil) == 0) {
					lua_pop(L, 1);
					lua_pushnil(L);
				}
			}

			if (lua_type(L, -1) == LUA_TTABLE) {
				/*
				 * Replace the serialized node
				 * with a new result, if it is a
				 * table.
				 */
				lua_replace(L, idx);
				return 1;
			}
		} else {
			/*
			 * Serializer don't use map with
			 * serialized objects. Copy object itself
			 * and call __serialize for it.
			 */
			lua_pushvalue(L, idx);
			if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
				diag_set(LuajitError, lua_tostring(L, -1));
				return -1;
			}
		}
		if (luaL_tofield(L, cfg, NULL, cache_index, -1, field) != 0)
			return -1;
		lua_replace(L, idx);
		return 0;
	}
	if (!lua_isstring(L, -1)) {
		diag_set(LuajitError, "invalid " LUAL_SERIALIZE " value");
		return -1;
	}
	const char *type = lua_tostring(L, -1);
	if (strcmp(type, "array") == 0 || strcmp(type, "seq") == 0 ||
	    strcmp(type, "sequence") == 0) {
		field->type = MP_ARRAY; /* Override type */
		field->size = luaL_arrlen(L, idx);
		/* YAML: use flow mode if __serialize == 'seq' */
		if (cfg->has_compact && type[3] == '\0')
			field->compact = true;
	} else if (strcmp(type, "map") == 0 || strcmp(type, "mapping") == 0) {
		field->type = MP_MAP;   /* Override type */
		field->size = luaL_maplen(L, idx);
		/* YAML: use flow mode if __serialize == 'map' */
		if (cfg->has_compact && type[3] == '\0')
			field->compact = true;
	} else {
		diag_set(LuajitError, "invalid " LUAL_SERIALIZE " value");
		return -1;
	}
	/* Remove value set by luaL_getmetafield. */
	lua_pop(L, 1);
	return 0;
}

static int
lua_field_inspect_table(struct lua_State *L, struct luaL_serializer *cfg,
			int cache_index, int idx, struct luaL_field *field)
{
	assert(lua_type(L, idx) == LUA_TTABLE);
	uint32_t size = 0;
	uint32_t max = 0;

	if (cfg->encode_load_metatables) {
		int top = lua_gettop(L);
		int res = lua_field_try_serialize(L, cfg, cache_index, idx, field);
		if (res == -1)
			return -1;
		assert(lua_gettop(L) == top);
		(void)top;
		if (res == 0)
			return 0;
		/* Fallthrough with res == 1 */
	}

	field->type = MP_ARRAY;

	/* Calculate size and check that table can represent an array */
	lua_pushnil(L);
	while (lua_next(L, idx)) {
		size++;
		lua_pop(L, 1); /* pop the value */
		lua_Number k;
		if (lua_type(L, -1) != LUA_TNUMBER ||
		    ((k = lua_tonumber(L, -1)) != size &&
		     (k < 1 || floor(k) != k))) {
			/* Finish size calculation */
			while (lua_next(L, idx)) {
				size++;
				lua_pop(L, 1); /* pop the value */
			}
			field->type = MP_MAP;
			field->size = size;
			return 0;
		}
		if (k > max)
			max = k;
	}

	/* Encode excessively sparse arrays as objects (if enabled) */
	if (cfg->encode_sparse_ratio > 0 &&
	    max > size * (uint32_t)cfg->encode_sparse_ratio &&
	    max > (uint32_t)cfg->encode_sparse_safe) {
		if (!cfg->encode_sparse_convert) {
			diag_set(LuajitError, "excessively sparse array");
			return -1;
		}
		field->type = MP_MAP;
		field->size = size;
		return 0;
	}

	assert(field->type == MP_ARRAY);
	field->size = max;
	return 0;
}

static void
lua_field_tostring(struct lua_State *L, struct luaL_serializer *cfg,
		   int cache_index, int idx, struct luaL_field *field)
{
	int top = lua_gettop(L);
	lua_getglobal(L, "tostring");
	lua_pushvalue(L, idx);
	lua_call(L, 1, 1);
	lua_replace(L, idx);
	lua_settop(L, top);
	if (luaL_tofield(L, cfg, NULL, cache_index, idx, field) < 0)
		luaT_error(L);
}

int
luaL_tofield(struct lua_State *L, struct luaL_serializer *cfg,
	     const struct serializer_opts *opts, int cache_index, int index,
	     struct luaL_field *field)
{
	if (index < 0)
		index = lua_gettop(L) + index + 1;

	double num;
	double intpart;
	size_t size;

#define CHECK_NUMBER(x) ({							\
	if (!isfinite(x) && !cfg->encode_invalid_numbers) {			\
		if (!cfg->encode_invalid_as_nil) {				\
			diag_set(LuajitError, "number must not be NaN or Inf");	\
			return -1;						\
		}								\
		field->type = MP_NIL;						\
	}})

	switch (lua_type(L, index)) {
	case LUA_TNUMBER:
		num = lua_tonumber(L, index);
		if (isfinite(num) && modf(num, &intpart) != 0.0) {
			field->type = MP_DOUBLE;
			field->dval = num;
		} else if (num >= 0 && num < exp2(64)) {
			field->type = MP_UINT;
			field->ival = (uint64_t) num;
		} else if (num >= -exp2(63) && num < exp2(63)) {
			field->type = MP_INT;
			field->ival = (int64_t) num;
		} else {
			field->type = MP_DOUBLE;
			field->dval = num;
			CHECK_NUMBER(num);
		}
		return 0;
	case LUA_TCDATA:
	{
		GCcdata *cd = cdataV(L->base + index - 1);
		void *cdata = (void *)cdataptr(cd);

		int64_t ival;
		switch (cd->ctypeid) {
		case CTID_BOOL:
			field->type = MP_BOOL;
			field->bval = *(bool*) cdata;
			return 0;
		case CTID_CCHAR:
		case CTID_INT8:
			ival = *(int8_t *) cdata;
			field->type = (ival >= 0) ? MP_UINT : MP_INT;
			field->ival = ival;
			return 0;
		case CTID_INT16:
			ival = *(int16_t *) cdata;
			field->type = (ival >= 0) ? MP_UINT : MP_INT;
			field->ival = ival;
			return 0;
		case CTID_INT32:
			ival = *(int32_t *) cdata;
			field->type = (ival >= 0) ? MP_UINT : MP_INT;
			field->ival = ival;
			return 0;
		case CTID_INT64:
			ival = *(int64_t *) cdata;
			field->type = (ival >= 0) ? MP_UINT : MP_INT;
			field->ival = ival;
			return 0;
		case CTID_UINT8:
			field->type = MP_UINT;
			field->ival = *(uint8_t *) cdata;
			return 0;
		case CTID_UINT16:
			field->type = MP_UINT;
			field->ival = *(uint16_t *) cdata;
			return 0;
		case CTID_UINT32:
			field->type = MP_UINT;
			field->ival = *(uint32_t *) cdata;
			return 0;
		case CTID_UINT64:
			field->type = MP_UINT;
			field->ival = *(uint64_t *) cdata;
			return 0;
		case CTID_FLOAT:
			field->type = MP_FLOAT;
			field->fval = *(float *) cdata;
			CHECK_NUMBER(field->fval);
			return 0;
		case CTID_DOUBLE:
			field->type = MP_DOUBLE;
			field->dval = *(double *) cdata;
			CHECK_NUMBER(field->dval);
			return 0;
		case CTID_P_CVOID:
		case CTID_P_VOID:
			if (*(void **) cdata == NULL) {
				field->type = MP_NIL;
				return 0;
			}
			/* Fall through */
		default:
			field->type = MP_EXT;
			if (cd->ctypeid == CTID_DECIMAL) {
				field->ext_type = MP_DECIMAL;
				field->decval = (decimal_t *) cdata;
			} else if (cd->ctypeid == CTID_UUID) {
				field->ext_type = MP_UUID;
				field->uuidval = (struct tt_uuid *) cdata;
			} else if (cd->ctypeid == CTID_CONST_STRUCT_ERROR_REF &&
				   opts != NULL &&
				   opts->error_marshaling_enabled) {
				field->ext_type = MP_ERROR;
			} else {
				field->ext_type = MP_UNKNOWN_EXTENSION;
			}
		}
		return 0;
	}
	case LUA_TBOOLEAN:
		field->type = MP_BOOL;
		field->bval = lua_toboolean(L, index);
		return 0;
	case LUA_TNIL:
		field->type = MP_NIL;
		return 0;
	case LUA_TSTRING:
		field->sval.data = lua_tolstring(L, index, &size);
		field->sval.len = (uint32_t) size;
		field->type = MP_STR;
		return 0;
	case LUA_TTABLE:
	{
		field->compact = false;
		return lua_field_inspect_table(L, cfg, cache_index, index,
					       field);
	}
	case LUA_TLIGHTUSERDATA:
	case LUA_TUSERDATA:
		field->sval.data = NULL;
		field->sval.len = 0;
		if (lua_touserdata(L, index) == NULL) {
			field->type = MP_NIL;
			return 0;
		}
		/* Fall through */
	default:
		field->type = MP_EXT;
		field->ext_type = MP_UNKNOWN_EXTENSION;
	}
#undef CHECK_NUMBER
	return 0;
}

void
luaL_convertfield(struct lua_State *L, struct luaL_serializer *cfg, int cache_index,
		  int idx, struct luaL_field *field)
{
	if (idx < 0)
		idx = lua_gettop(L) + idx + 1;
	assert(field->type == MP_EXT && field->ext_type == MP_UNKNOWN_EXTENSION); /* must be called after tofield() */

	if (cfg->encode_load_metatables) {
		int type = lua_type(L, idx);
		if (type == LUA_TCDATA) {
			/*
			 * Don't call __serialize on primitive types
			 * https://github.com/tarantool/tarantool/issues/1226
			 */
			GCcdata *cd = cdataV(L->base + idx - 1);
			if (cd->ctypeid > CTID_CTYPEID)
				lua_field_inspect_ucdata(L, cfg, cache_index, idx, field);
		} else if (type == LUA_TUSERDATA) {
			lua_field_inspect_ucdata(L, cfg, cache_index, idx, field);
		}
	}

	if (field->type == MP_EXT && field->ext_type == MP_UNKNOWN_EXTENSION &&
	    cfg->encode_use_tostring)
		lua_field_tostring(L, cfg, 0, idx, field);

	if (field->type != MP_EXT || field->ext_type != MP_UNKNOWN_EXTENSION)
		return;

	if (cfg->encode_invalid_as_nil) {
		field->type = MP_NIL;
		return;
	}

	luaL_error(L, "unsupported Lua type '%s'",
		   lua_typename(L, lua_type(L, idx)));
}

/* }}} Fill luaL_field */

int
tarantool_lua_serializer_init(struct lua_State *L)
{
	static const struct luaL_Reg serializermeta[] = {
		{NULL, NULL},
	};
	luaL_register_type(L, LUAL_SERIALIZER, serializermeta);

	lua_createtable(L, 0, 1);
	lua_pushliteral(L, "map"); /* YAML will use flow mode */
	lua_setfield(L, -2, LUAL_SERIALIZE);
	/* automatically reset hints on table change */
	luaL_loadstring(L, "setmetatable((...), nil); return rawset(...)");
	lua_setfield(L, -2, "__newindex");
	luaL_map_metatable_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_createtable(L, 0, 1);
	lua_pushliteral(L, "seq"); /* YAML will use flow mode */
	lua_setfield(L, -2, LUAL_SERIALIZE);
	/* automatically reset hints on table change */
	luaL_loadstring(L, "setmetatable((...), nil); return rawset(...)");
	lua_setfield(L, -2, "__newindex");
	luaL_array_metatable_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	return 0;
}
