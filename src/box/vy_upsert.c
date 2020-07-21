/*
 * Copyright 2010-2017, Tarantool AUTHORS, please see AUTHORS file.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "vy_upsert.h"

#include <sys/uio.h>
#include <small/region.h>
#include <msgpuck/msgpuck.h>
#include "vy_stmt.h"
#include "xrow_update.h"
#include "fiber.h"
#include "column_mask.h"

/**
 * Try to squash two upsert series (msgspacked index_base + ops)
 * Try to create a tuple with squahed operations
 *
 * @retval 0 && *result_stmt != NULL : successful squash
 * @retval 0 && *result_stmt == NULL : unsquashable sources
 * @retval -1 - memory error
 */
static int
vy_upsert_try_to_squash(struct tuple_format *format,
			const char *key_mp, const char *key_mp_end,
			const char *old_serie, const char *old_serie_end,
			const char *new_serie, const char *new_serie_end,
			struct tuple **result_stmt)
{
	*result_stmt = NULL;

	size_t squashed_size;
	const char *squashed =
		xrow_upsert_squash(old_serie, old_serie_end,
				   new_serie, new_serie_end, format,
				   &squashed_size, 0);
	if (squashed == NULL)
		return 0;
	/* Successful squash! */
	struct iovec operations[1];
	operations[0].iov_base = (void *)squashed;
	operations[0].iov_len = squashed_size;

	*result_stmt = vy_stmt_new_upsert(format, key_mp, key_mp_end,
					  operations, 1, false);
	if (*result_stmt == NULL)
		return -1;
	return 0;
}

/**
 * Check that key hasn't been changed after applying upsert operation.
 */
static bool
vy_apply_result_does_cross_pk(struct tuple *old_stmt, const char *result,
			      const char *result_end, struct key_def *cmp_def,
			      uint64_t col_mask)
{
	if (!key_update_can_be_skipped(cmp_def->column_mask, col_mask)) {
		struct tuple *tuple =
			vy_stmt_new_replace(tuple_format(old_stmt), result,
					    result_end);
		int cmp_res = vy_stmt_compare(old_stmt, HINT_NONE, tuple,
					       HINT_NONE, cmp_def);
		tuple_unref(tuple);
		return cmp_res != 0;
	}
	return false;
}

/**
 * Apply update operations stored in @new_stmt (which is assumed to
 * be upsert statement) on tuple @old_stmt. If @old_stmt is void
 * statement (i.e. it is NULL or delete statement) then operations
 * are applied on tuple @new_stmt. All operations which can't be
 * applied are skipped; errors may be logged depending on @supress_error
 * flag.
 *
 * @upsert Upsert statement to be applied on @stmt.
 * @stmt Statement to be used as base for upsert operations.
 * @cmp_def Key definition required to provide check of primary key
 *          modification.
 * @retrun Tuple containing result of upsert application;
 *         NULL in case OOM.
 */
static struct tuple *
vy_apply_upsert_on_terminal_stmt(struct tuple *upsert, struct tuple *stmt,
				 struct key_def *cmp_def, bool suppress_error)
{
	assert(vy_stmt_type(upsert) == IPROTO_UPSERT);
	assert(stmt == NULL || vy_stmt_type(stmt) != IPROTO_UPSERT);

	uint32_t mp_size;
	const char *new_ops = vy_stmt_upsert_ops(upsert, &mp_size);
	/* Msgpack containing result of upserts application. */
	const char *result_mp;
	if (vy_stmt_is_void(stmt))
		result_mp = vy_upsert_data_range(upsert, &mp_size);
	else
		result_mp = tuple_data_range(stmt, &mp_size);
	const char *result_mp_end = result_mp + mp_size;
	/*
	 * xrow_upsert_execute() allocates result using region,
	 * so save starting point to release it later.
	 */
	struct region *region = &fiber()->gc;
	size_t region_svp = region_used(region);
	uint64_t column_mask = COLUMN_MASK_FULL;
	struct tuple_format *format = tuple_format(upsert);

	uint32_t ups_cnt = mp_decode_array(&new_ops);
	const char *ups_ops = new_ops;
	/*
	 * In case upsert folds into insert, we must skip first
	 * update operations.
	 */
	if (vy_stmt_is_void(stmt)) {
		ups_cnt--;
		mp_next(&ups_ops);
	}
	for (uint32_t i = 0; i < ups_cnt; ++i) {
		assert(mp_typeof(*ups_ops) == MP_ARRAY);
		const char *ups_ops_end = ups_ops;
		mp_next(&ups_ops_end);
		const char *exec_res = result_mp;
		exec_res = xrow_upsert_execute(ups_ops, ups_ops_end, result_mp,
					       result_mp_end, format, &mp_size,
					       0, suppress_error, &column_mask);
		if (exec_res == NULL) {
			if (! suppress_error) {
				assert(diag_last_error(diag_get()) != NULL);
				struct error *e = diag_last_error(diag_get());
				/* Bail out immediately in case of OOM. */
				if (e->type != &type_ClientError) {
					region_truncate(region, region_svp);
					return NULL;
				}
				diag_log();
			}
			ups_ops = ups_ops_end;
			continue;
		}
		/*
		 * If it turns out that resulting tuple modifies primary
		 * key, than simply ignore this upsert.
		 */
		if (vy_apply_result_does_cross_pk(stmt, exec_res,
						  exec_res + mp_size, cmp_def,
						  column_mask)) {
			if (! suppress_error) {
				say_error("upsert operations %s are not applied"\
					  " due to primary key modification",
					  mp_str(ups_ops));
			}
			ups_ops = ups_ops_end;
			continue;
		}
		ups_ops = ups_ops_end;
		/*
		 * In case statement exists its format must
		 * satisfy space's format. Otherwise, upsert's
		 * tuple is checked to fit format once it is
		 * processed in vy_upsert().
		 */
		if (stmt != NULL) {
			if (tuple_validate_raw(tuple_format(stmt),
					       exec_res) != 0) {
				if (! suppress_error)
					diag_log();
				continue;
			}
		}
		result_mp = exec_res;
		result_mp_end = exec_res + mp_size;
	}
	struct tuple *new_terminal_stmt = vy_stmt_new_replace(format, result_mp,
							      result_mp_end);
	region_truncate(region, region_svp);
	if (new_terminal_stmt == NULL)
		return NULL;
	vy_stmt_set_lsn(new_terminal_stmt, vy_stmt_lsn(upsert));
	return new_terminal_stmt;
}

static bool
tuple_format_is_suitable_for_squash(struct tuple_format *format)
{
	struct tuple_field *field;
	json_tree_foreach_entry_preorder(field, &format->fields.root,
					 struct tuple_field, token) {
		if (field->type == FIELD_TYPE_UNSIGNED)
				return false;
	}
	return true;
}

/**
 * Unpack upsert's update operations from msgpack array
 * into array of iovecs.
 */
static void
upsert_ops_to_iovec(const char *ops, uint32_t ops_cnt, struct iovec *iov_arr)
{
	for (uint32_t i = 0; i < ops_cnt; ++i) {
		assert(mp_typeof(*ops) == MP_ARRAY);
		iov_arr[i].iov_base = (char *) ops;
		mp_next(&ops);
		iov_arr[i].iov_len = ops - (char *) iov_arr[i].iov_base;
	}
}

struct tuple *
vy_apply_upsert(struct tuple *new_stmt, struct tuple *old_stmt,
		struct key_def *cmp_def, bool suppress_error)
{
	/*
	 * old_stmt - previous (old) version of stmt
	 * new_stmt - next (new) version of stmt
	 * result_stmt - the result of merging new and old
	 */
	assert(new_stmt != NULL);
	assert(new_stmt != old_stmt);
	assert(vy_stmt_type(new_stmt) == IPROTO_UPSERT);

	struct tuple *result_stmt = NULL;
	if (old_stmt == NULL || vy_stmt_type(old_stmt) != IPROTO_UPSERT) {
		return vy_apply_upsert_on_terminal_stmt(new_stmt, old_stmt,
						        cmp_def, suppress_error);
	}

	assert(vy_stmt_type(old_stmt) == IPROTO_UPSERT);
	/*
	 * Unpack UPSERT operation from the old and new stmts.
	 */
	assert(old_stmt != NULL);
	uint32_t mp_size;
	const char *old_ops = vy_stmt_upsert_ops(old_stmt, &mp_size);
	const char *old_ops_end = old_ops + mp_size;
	assert(old_ops_end > old_ops);
	const char *old_stmt_mp = vy_upsert_data_range(old_stmt, &mp_size);
	const char *old_stmt_mp_end = old_stmt_mp + mp_size;
	const char *new_ops = vy_stmt_upsert_ops(new_stmt, &mp_size);

	/*
	 * UPSERT + UPSERT case: squash arithmetic operations.
	 * Note that we can process this only in case result
	 * can't break format under no circumstances. Since
	 * subtraction can lead to negative values, unsigned
	 * field are considered to be inappropriate.
	 */
	struct tuple_format *format = tuple_format(old_stmt);
	struct region *region = &fiber()->gc;
	size_t region_svp = region_used(region);
	if (tuple_format_is_suitable_for_squash(format)) {
		const char *new_ops_end = new_ops + mp_size;
		if (vy_upsert_try_to_squash(format, old_stmt_mp, old_stmt_mp_end,
					    old_ops, old_ops_end, new_ops,
					    new_ops_end, &result_stmt) != 0) {
			/* OOM */
			region_truncate(region, region_svp);
			return NULL;
		}
	}
	/*
	 * Adding update operations. We keep order of update operations in
	 * the array the same. It is vital since first set of operations
	 * must be skipped in case upsert folds into insert. For instance:
	 * old_ops = {{{op1}, {op2}}, {{op3}}}
	 * new_ops = {{{op4}, {op5}}}
	 * res_ops = {{{op1}, {op2}}, {{op3}}, {{op4}, {op5}}}
	 * If upsert corresponding to old_ops becomes insert, then
	 * {{op1}, {op2}} update operations are not applied.
	 */
	uint32_t old_ops_cnt = mp_decode_array(&old_ops);
	uint32_t new_ops_cnt = mp_decode_array(&new_ops);
	size_t ops_size = sizeof(struct iovec) * (old_ops_cnt + new_ops_cnt);
	struct iovec *operations = region_alloc(region, ops_size);
	if (operations == NULL) {
		region_truncate(region, region_svp);
		diag_set(OutOfMemory, ops_size, "region_alloc", "operations");
		return NULL;
	}
	upsert_ops_to_iovec(old_ops, old_ops_cnt, operations);
	upsert_ops_to_iovec(new_ops, new_ops_cnt, &operations[old_ops_cnt]);

	result_stmt = vy_stmt_new_upsert(format, old_stmt_mp, old_stmt_mp_end,
					 operations, old_ops_cnt + new_ops_cnt,
					 false);
	region_truncate(region, region_svp);
	if (result_stmt == NULL)
		return NULL;
	vy_stmt_set_lsn(result_stmt, vy_stmt_lsn(new_stmt));

	return result_stmt;
}
