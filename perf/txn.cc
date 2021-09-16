#include <iostream>
#include <benchmark/benchmark.h>

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#include "module.h"

#include <lua.h>
#include <lauxlib.h>
#include <msgpuck.h>

#define TXN_PENDING_MAX 10000

static int
fiber_function(va_list ap)
{
	box_error_t *error;
	benchmark::State *state_ = va_arg(ap, benchmark::State *);
	struct fiber_cond *cond = va_arg(ap, struct fiber_cond *);

	if (box_txn_begin() != 0) {
		error = box_error_last();
		state_->SkipWithError(box_error_message(error));
		return -1;
	}
	if (fiber_cond_wait(cond) != 0) {
		error = box_error_last();
		state_->SkipWithError(box_error_message(error));
		return -1;
	}
	if (box_txn_commit() != 0) {
		error = box_error_last();
		state_->SkipWithError(box_error_message(error));
		return -1;
	}
	return 0;
}

class TxnPendings {
public:
	TxnPendings(benchmark::State& state, uint32_t space_id)
	{
		box_error_t *error;
		state_ = &state;
		space_id_ = space_id;
		cond = fiber_cond_new();
		if (cond == NULL) {
			error = box_error_last();
			state_->SkipWithError(box_error_message(error));
			goto finish;
		}
		for (count = 0; count < TXN_PENDING_MAX; count++) {
			fibers[count] = fiber_new("fiber", fiber_function);
			if (fibers[count] == NULL) {
				error = box_error_last();
				state_->SkipWithError(box_error_message(error));
				break;
			}
			fiber_set_joinable(fibers[count], true);
			fiber_start(fibers[count], state_, cond);
		}
	finish:;
	}
	~TxnPendings()
	{
		box_error_t *error;
		fiber_cond_broadcast(cond);
		for (unsigned i = 0; i < count; i++) {
			if (fiber_join(fibers[i]) != 0) {
				error = box_error_last();
				state_->SkipWithError(box_error_message(error));
			}
		}
		fiber_cond_delete(cond);
		if (box_truncate(space_id_) != 0) {
			error = box_error_last();
			state_->SkipWithError(box_error_message(error));
		}
	}
private:
	uint32_t space_id_;
	unsigned count;
	struct fiber_cond *cond;
	struct fiber *fibers[TXN_PENDING_MAX];
	benchmark::State *state_;
};

static void
show_warning_if_debug()
{
#ifndef NDEBUG
	std::cerr << "#######################################################\n"
		  << "#######################################################\n"
		  << "#######################################################\n"
		  << "###                                                 ###\n"
		  << "###                    WARNING!                     ###\n"
		  << "###   The performance test is run in debug build!   ###\n"
		  << "###   Test results are definitely inappropriate!    ###\n"
		  << "###                                                 ###\n"
		  << "#######################################################\n"
		  << "#######################################################\n"
		  << "#######################################################\n";
#endif // #ifndef NDEBUG
}

static uint32_t space_id;
static uint32_t index_id;

static void
bench_txn_simple(benchmark::State& state)
{
	TxnPendings init(state, space_id);
	for (auto _ : state) {
		box_txn_begin();
		box_txn_commit();
	}
}
BENCHMARK(bench_txn_simple);

static void
bench_txn_insert(benchmark::State& state)
{
	TxnPendings init(state, space_id);
	char data[mp_sizeof_array(1) + mp_sizeof_uint((uint64_t)(~0))];
	box_error_t *error;
	uint64_t i = 0;
	for (auto _ : state) {
		char *data_end = data;
		data_end = mp_encode_array(data_end, 1);
		data_end = mp_encode_uint(data_end, i++);
		if (box_insert(space_id, data, data_end, NULL) < 0) {
			error = box_error_last();
			state.SkipWithError(box_error_message(error));
		}
	}
}
BENCHMARK(bench_txn_insert);

static void
bench_txn_replace(benchmark::State& state)
{
	TxnPendings init(state, space_id);
	char data[mp_sizeof_array(1) + mp_sizeof_uint((uint64_t)(~0))];
	box_error_t *error;
	uint64_t i = 0;
	for (auto _ : state) {
		char *data_end = data;
		data_end = mp_encode_array(data_end, 1);
		data_end = mp_encode_uint(data_end, i++);
		if (box_replace(space_id, data, data_end, NULL) < 0) {
			error = box_error_last();
			state.SkipWithError(box_error_message(error));
		}
	}
}
BENCHMARK(bench_txn_replace);

static void
bench_txn_delete(benchmark::State& state)
{
	TxnPendings init(state, space_id);
	char data[mp_sizeof_array(1) + mp_sizeof_uint((uint64_t)(~0))];
	box_error_t *error;
	uint64_t i = 0;
	for (auto _ : state) {
		char *data_end = data;
		data_end = mp_encode_array(data_end, 1);
		data_end = mp_encode_uint(data_end, i++);
		if (box_replace(space_id, data, data_end, NULL) < 0) {
			error = box_error_last();
			state.SkipWithError(box_error_message(error));
		}
		if (box_delete(space_id, index_id, data, data_end, NULL) < 0) {
			error = box_error_last();
			state.SkipWithError(box_error_message(error));
		}
	}
}
BENCHMARK(bench_txn_delete);

static int
bench(lua_State *L)
{
	const char *fmt;
	show_warning_if_debug();
	if (!lua_istable(L, 1)) {
		fmt = "Use txn:bench(...) instead of txn.bench(...)";
		luaL_error(L, fmt);
	}
	const char *space_name = lua_tostring(L, 2);
	const char *index_name = lua_tostring(L, 3);
	if (space_name == NULL || index_name == NULL) {
		fmt = "Use txn:bench(\"space_name\", \"index_name\")";
		luaL_error(L, fmt);
	}
	space_id = box_space_id_by_name(space_name, strlen(space_name));
	index_id = box_index_id_by_name(space_id, index_name, strlen(index_name));
	if (space_id == BOX_ID_NIL || index_id == BOX_ID_NIL) {
		fmt = "Can't find index %s in space %s";
		luaL_error(L, fmt, index_name, space_name);
	}
	::benchmark::RunSpecifiedBenchmarks();
	return 0;
}

static const struct luaL_Reg lib[] = {
	{"bench", bench},
	{NULL, NULL},
};

LUA_API
int
luaopen_txn(lua_State *L)
{
	luaL_register(L, "txn", lib);
	return 0;
}

#if defined(__cplusplus)
} /* extern "C" */
#endif /* defined(__cplusplus) */