/* SPDX-License-Identifier: GPL-3.0-only */
#include "vfs.h"

typedef struct tmpfs_file_data {
  size_t capacity;
  uint8_t *bytes;
} tmpfs_file_data;
struct tmpfs_entry {
  tmpfs_entry *next;
  char path[256];
  vfs_kind kind;
  uint64_t size;
  tmpfs_file_data data;
};

static int bytes_equal(const void *left, const void *right, size_t size) {
  const uint8_t *a = left, *b = right;
  for (size_t i = 0; i < size; ++i)
    if (a[i] != b[i])
      return 0;
  return 1;
}
static void bytes_copy(void *target, const void *source, size_t size) {
  uint8_t *d = target;
  const uint8_t *s = source;
  for (size_t i = 0; i < size; ++i)
    d[i] = s[i];
}
static size_t path_len(const char *s) {
  size_t n = 0;
  while (s && s[n]) {
    if (++n > 255)
      return 256;
  }
  return n;
}
static int valid_path(const char *p) {
  if (!p || p[0] != '/')
    return 0;
  size_t n = path_len(p);
  if (!n || n > 255)
    return 0;
  for (size_t i = 0; i < n; i++)
    if (p[i] == '\\')
      return 0;
  return 1;
}
static vfs_mount *mount_for(vfs *fs, const char *path) {
  vfs_mount *best = 0;
  size_t bestn = 0;
  for (vfs_mount *m = fs->mounts; m; m = m->next) {
    size_t n = path_len(m->path);
    if (n >= bestn && bytes_equal(path, m->path, n) &&
        (path[n] == 0 || path[n] == '/' || n == 1)) {
      best = m;
      bestn = n;
    }
  }
  return best;
}
static const char *relative(vfs_mount *m, const char *p) {
  if (path_len(m->path) == 1)
    return p;
  p += path_len(m->path);
  return *p ? p : "/";
}
static uintptr_t mount_lock(vfs_mount *m) {
  return m->memory.lock ? m->memory.lock(m->memory.context) : 0;
}
static void mount_unlock(vfs_mount *m, uintptr_t flags) {
  if (m->memory.unlock)
    m->memory.unlock(m->memory.context, flags);
}
vfs_status vfs_init(vfs *fs, const vfs_memory *mem) {
  if (!fs || !mem || !mem->allocate || !mem->deallocate)
    return VFS_INVALID;
  *fs = (vfs){.memory = *mem};
  return VFS_OK;
}
vfs_status vfs_mount_fs(vfs *fs, const char *path, const vfs_ops *ops,
                        void *ctx, vfs_mount *out) {
  if (!fs || !valid_path(path) || !ops || !ops->lookup || !out)
    return VFS_INVALID;
  for (vfs_mount *m = fs->mounts; m; m = m->next)
    if (bytes_equal(m->path, path, path_len(path) + 1))
      return VFS_EXISTS;
  *out = (vfs_mount){.ops = ops, .context = ctx, .mounted = 1};
  out->memory = fs->memory;
  size_t n = path_len(path);
  if (n >= sizeof(out->path))
    return VFS_TOO_LARGE;
  bytes_copy(out->path, path, n + 1);
  out->next = fs->mounts;
  fs->mounts = out;
  return VFS_OK;
}
vfs_status vfs_unmount(vfs *fs, const char *path) {
  if (!fs || !valid_path(path))
    return VFS_INVALID;
  vfs_mount **at = &fs->mounts;
  while (*at && !bytes_equal((*at)->path, path, path_len(path) + 1))
    at = &(*at)->next;
  if (!*at)
    return VFS_NOT_FOUND;
  vfs_mount *m = *at;
  uintptr_t flags = mount_lock(m);
  if (m->open_files) {
    mount_unlock(m, flags);
    return VFS_BUSY;
  }
  if (m->ops->sync && m->ops->sync(m->context) != VFS_OK) {
    mount_unlock(m, flags);
    return VFS_IO_ERROR;
  }
  *at = m->next;
  mount_unlock(m, flags);
  if (m->ops->destroy)
    m->ops->destroy(m->context);
  m->mounted = 0;
  return VFS_OK;
}
vfs_status vfs_lookup(vfs *fs, const char *p, vfs_node *out) {
  if (!fs || !valid_path(p) || !out)
    return VFS_INVALID;
  vfs_mount *m = mount_for(fs, p);
  if (!m)
    return VFS_NOT_FOUND;
  uintptr_t flags = mount_lock(m);
  vfs_status s = m->ops->lookup(m->context, relative(m, p), out);
  mount_unlock(m, flags);
  return s;
}
vfs_status vfs_create(vfs *fs, const char *p, vfs_kind k) {
  if (!fs || !valid_path(p))
    return VFS_INVALID;
  vfs_mount *m = mount_for(fs, p);
  if (!m || !m->ops->create)
    return VFS_READ_ONLY;
  uintptr_t flags = mount_lock(m);
  vfs_status s = m->ops->create(m->context, relative(m, p), k);
  mount_unlock(m, flags);
  return s;
}
vfs_status vfs_open(vfs *fs, const char *p, int wr, vfs_file *out) {
  if (!fs || !valid_path(p) || !out)
    return VFS_INVALID;
  vfs_mount *m = mount_for(fs, p);
  if (!m)
    return VFS_NOT_FOUND;
  uintptr_t flags = mount_lock(m);
  vfs_node node;
  vfs_status s = m->ops->lookup(m->context, relative(m, p), &node);
  if (s != VFS_OK) {
    mount_unlock(m, flags);
    return s;
  }
  if (node.kind != VFS_FILE) {
    mount_unlock(m, flags);
    return VFS_IS_DIRECTORY;
  }
  if (wr && node.read_only) {
    mount_unlock(m, flags);
    return VFS_READ_ONLY;
  }
  size_t n = path_len(p);
  if (n >= sizeof(out->path)) {
    mount_unlock(m, flags);
    return VFS_TOO_LARGE;
  }
  *out = (vfs_file){.mount = m, .writable = !!wr};
  bytes_copy(out->path, p, n + 1);
  ++m->open_files;
  mount_unlock(m, flags);
  return VFS_OK;
}
vfs_status vfs_close(vfs_file *f) {
  if (!f || !f->mount)
    return VFS_INVALID;
  uintptr_t flags = mount_lock(f->mount);
  if (f->mount->open_files)
    --f->mount->open_files;
  mount_unlock(f->mount, flags);
  *f = (vfs_file){0};
  return VFS_OK;
}
vfs_status vfs_read(vfs_file *f, void *b, size_t n, size_t *done) {
  if (!f || !f->mount || !done || (!b && n) || !f->mount->ops->read)
    return VFS_INVALID;
  *done = 0;
  uintptr_t flags = mount_lock(f->mount);
  vfs_status s = f->mount->ops->read(
      f->mount->context, relative(f->mount, f->path), f->offset, b, n, done);
  if (s == VFS_OK)
    f->offset += *done;
  mount_unlock(f->mount, flags);
  return s;
}
vfs_status vfs_write(vfs_file *f, const void *b, size_t n, size_t *done) {
  if (!f || !f->mount || !f->writable || !done || (!b && n) ||
      !f->mount->ops->write)
    return VFS_INVALID;
  *done = 0;
  uintptr_t flags = mount_lock(f->mount);
  vfs_status s = f->mount->ops->write(
      f->mount->context, relative(f->mount, f->path), f->offset, b, n, done);
  if (s == VFS_OK)
    f->offset += *done;
  mount_unlock(f->mount, flags);
  return s;
}
vfs_status vfs_remove(vfs *fs, const char *p) {
  if (!fs || !valid_path(p))
    return VFS_INVALID;
  vfs_mount *m = mount_for(fs, p);
  if (!m || !m->ops->remove)
    return VFS_READ_ONLY;
  uintptr_t flags = mount_lock(m);
  vfs_status s = m->ops->remove(m->context, relative(m, p));
  mount_unlock(m, flags);
  return s;
}
vfs_status vfs_rename(vfs *fs, const char *a, const char *b) {
  if (!fs || !valid_path(a) || !valid_path(b))
    return VFS_INVALID;
  vfs_mount *x = mount_for(fs, a), *y = mount_for(fs, b);
  if (!x || x != y)
    return VFS_INVALID;
  if (!x->ops->rename)
    return VFS_READ_ONLY;
  uintptr_t flags = mount_lock(x);
  vfs_status s = x->ops->rename(x->context, relative(x, a), relative(y, b));
  mount_unlock(x, flags);
  return s;
}
vfs_status vfs_sync(vfs *fs) {
  if (!fs)
    return VFS_INVALID;
  for (vfs_mount *m = fs->mounts; m; m = m->next)
    if (m->ops->sync) {
      uintptr_t flags = mount_lock(m);
      vfs_status s = m->ops->sync(m->context);
      mount_unlock(m, flags);
      if (s != VFS_OK)
        return s;
    }
  return VFS_OK;
}
void vfs_destroy(vfs *fs) {
  if (!fs)
    return;
  while (fs->mounts) {
    vfs_mount *m = fs->mounts;
    fs->mounts = m->next;
    if (m->ops->destroy)
      m->ops->destroy(m->context);
    m->mounted = 0;
  }
  fs->mounts = 0;
}

static tmpfs_entry *find(tmpfs *t, const char *p) {
  for (tmpfs_entry *e = t->entries; e; e = e->next)
    if (bytes_equal(e->path, p, path_len(p) + 1))
      return e;
  return 0;
}
static int parent_exists(tmpfs *t, const char *p) {
  if (p[0] == '/' && p[1] == 0)
    return 1;
  size_t n = path_len(p);
  while (n > 1 && p[n - 1] != '/')
    --n;
  if (n <= 1)
    return find(t, "/") != 0;
  char q[256];
  bytes_copy(q, p, n - 1);
  q[n - 1] = 0;
  tmpfs_entry *e = find(t, q);
  return e && e->kind == VFS_DIRECTORY;
}
static vfs_status tlookup(void *c, const char *p, vfs_node *out) {
  tmpfs *t = c;
  tmpfs_entry *e = find(t, p);
  if (!e)
    return VFS_NOT_FOUND;
  *out = (vfs_node){e->kind, e->size, 0};
  return VFS_OK;
}
static vfs_status tcreate(void *c, const char *p, vfs_kind k) {
  tmpfs *t = c;
  if (!p || p[0] != '/' || find(t, p))
    return VFS_EXISTS;
  if (!parent_exists(t, p))
    return VFS_NOT_DIRECTORY;
  if (t->count >= t->limit)
    return VFS_NO_MEMORY;
  tmpfs_entry *e = t->memory.allocate(t->memory.context, sizeof(*e));
  if (!e)
    return VFS_NO_MEMORY;
  *e = (tmpfs_entry){.kind = k, .next = t->entries};
  size_t n = path_len(p);
  if (n >= sizeof(e->path)) {
    t->memory.deallocate(t->memory.context, e);
    return VFS_TOO_LARGE;
  }
  bytes_copy(e->path, p, n + 1);
  t->entries = e;
  ++t->count;
  return VFS_OK;
}
static vfs_status tread(void *c, const char *p, uint64_t off, void *b, size_t n,
                        size_t *done) {
  tmpfs_entry *e = find(c, p);
  if (!e)
    return VFS_NOT_FOUND;
  if (e->kind != VFS_FILE)
    return VFS_IS_DIRECTORY;
  *done = 0;
  if (off >= e->size)
    return VFS_OK;
  size_t take = (size_t)(e->size - off);
  if (take > n)
    take = n;
  bytes_copy(b, e->data.bytes + (size_t)off, take);
  *done = take;
  return VFS_OK;
}
static vfs_status twrite(void *c, const char *p, uint64_t off, const void *b,
                         size_t n, size_t *done) {
  tmpfs *t = c;
  tmpfs_entry *e = find(t, p);
  if (!e)
    return VFS_NOT_FOUND;
  if (e->kind != VFS_FILE)
    return VFS_IS_DIRECTORY;
  if (off > SIZE_MAX || n > SIZE_MAX - (size_t)off)
    return VFS_TOO_LARGE;
  size_t end = (size_t)off + n;
  if (end > e->data.capacity) {
    size_t cap = e->data.capacity ? e->data.capacity : 64;
    while (cap < end) {
      if (cap > SIZE_MAX / 2)
        return VFS_TOO_LARGE;
      cap *= 2;
    }
    uint8_t *q = t->memory.allocate(t->memory.context, cap);
    if (!q)
      return VFS_NO_MEMORY;
    for (size_t i = 0; i < e->size; i++)
      q[i] = e->data.bytes[i];
    for (size_t i = (size_t)e->size; i < cap; i++)
      q[i] = 0;
    if (e->data.bytes)
      t->memory.deallocate(t->memory.context, e->data.bytes);
    e->data.bytes = q;
    e->data.capacity = cap;
  }
  bytes_copy(e->data.bytes + (size_t)off, b, n);
  if (end > e->size)
    e->size = end;
  *done = n;
  return VFS_OK;
}
static vfs_status tremove(void *c, const char *p) {
  tmpfs *t = c;
  if (bytes_equal(p, "/", 2))
    return VFS_BUSY;
  tmpfs_entry **at = &t->entries;
  while (*at && !bytes_equal((*at)->path, p, path_len(p) + 1))
    at = &(*at)->next;
  if (!*at)
    return VFS_NOT_FOUND;
  tmpfs_entry *e = *at;
  if (e->kind == VFS_DIRECTORY) {
    size_t n = path_len(p);
    for (tmpfs_entry *x = t->entries; x; x = x->next)
      if (x != e && bytes_equal(x->path, p, n) && x->path[n] == '/')
        return VFS_BUSY;
  }
  *at = e->next;
  if (e->data.bytes)
    t->memory.deallocate(t->memory.context, e->data.bytes);
  t->memory.deallocate(t->memory.context, e);
  --t->count;
  return VFS_OK;
}
static vfs_status trename(void *c, const char *a, const char *b) {
  tmpfs *t = c;
  tmpfs_entry *e = find(t, a);
  if (!e)
    return VFS_NOT_FOUND;
  if (find(t, b))
    return VFS_EXISTS;
  if (!parent_exists(t, b))
    return VFS_NOT_DIRECTORY;
  size_t n = path_len(b);
  if (n >= sizeof(e->path))
    return VFS_TOO_LARGE;
  bytes_copy(e->path, b, n + 1);
  return VFS_OK;
}
static vfs_status tsync(void *c) {
  (void)c;
  return VFS_OK;
}
static void tdestroy(void *c) {
  tmpfs *t = c;
  if (!t)
    return;
  tmpfs_entry *e = t->entries;
  while (e) {
    tmpfs_entry *n = e->next;
    if (e->data.bytes)
      t->memory.deallocate(t->memory.context, e->data.bytes);
    t->memory.deallocate(t->memory.context, e);
    e = n;
  }
  t->entries = 0;
  t->count = 0;
}
const vfs_ops tmpfs_operations = {tlookup, tcreate, tread, twrite,
                                  tremove, trename, tsync, tdestroy};
vfs_status tmpfs_init(tmpfs *t, const vfs_memory *m, size_t limit) {
  if (!t || !m || !limit)
    return VFS_INVALID;
  *t = (tmpfs){.memory = *m, .limit = limit};
  return tcreate(t, "/", VFS_DIRECTORY);
}
