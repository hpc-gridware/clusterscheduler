#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

void fgl_rsv_sort(void);

void fgl_add_r(u_long32 root, bool rw);

void fgl_add_u(u_long32 root, u_long32 id, bool rw);

void fgl_add_s(u_long32 root, const char *id, bool rw);

void fgl_clear(void);

void fgl_dump(dstring *dstr);

void fgl_dump_stats(dstring *stats_str);

void fgl_lock(void);

void fgl_unlock(void);
