-- Journal mode of one SQLite database, with KOReader's SQLite (host side):
--   luajit v188-wal.lua DB status|wal|delete
-- Prints "WAL_DB mode=<mode> check=<quick_check> version=<sqlite>" after the
-- change. The mode is stored in the file (header bytes 18/19), so it
-- outlives this connection; closing the last connection removes -wal/-shm.
local ffi = require("ffi")
ffi.cdef[[
typedef struct sqlite3 sqlite3; typedef struct sqlite3_stmt sqlite3_stmt;
int sqlite3_open_v2(const char *, sqlite3 **, int, const char *);
int sqlite3_prepare_v2(sqlite3 *, const char *, int, sqlite3_stmt **, const char **);
int sqlite3_step(sqlite3_stmt *); const unsigned char *sqlite3_column_text(sqlite3_stmt *, int);
int sqlite3_finalize(sqlite3_stmt *); int sqlite3_close(sqlite3 *);
const char *sqlite3_errmsg(sqlite3 *); const char *sqlite3_libversion(void);
int sqlite3_busy_timeout(sqlite3 *, int);
]]
local lib = ffi.load(arg[4] or "/mnt/onboard/.adds/koreader/libs/libsqlite3.so.0")
local path, want = arg[1], arg[2]
local function die(msg) io.write("WAL_ERROR ", msg, "\n"); os.exit(1) end
local pdb = ffi.new("sqlite3 *[1]")
-- SQLITE_OPEN_READWRITE only: never create a database that is not there.
if lib.sqlite3_open_v2(path, pdb, 2, nil) ~= 0 then die("open " .. path) end
local db = pdb[0]; lib.sqlite3_busy_timeout(db, 5000)
local function one(sql)
  local st = ffi.new("sqlite3_stmt *[1]")
  if lib.sqlite3_prepare_v2(db, sql, -1, st, nil) ~= 0 then die(sql .. ": " .. ffi.string(lib.sqlite3_errmsg(db))) end
  local rc, v = lib.sqlite3_step(st[0]), nil
  if rc == 100 then local t = lib.sqlite3_column_text(st[0], 0); v = t ~= nil and ffi.string(t) or "" elseif rc ~= 101 then
    local e = ffi.string(lib.sqlite3_errmsg(db)); lib.sqlite3_finalize(st[0]); die(sql .. ": " .. e) end
  lib.sqlite3_finalize(st[0]); return v
end
local check = one("PRAGMA quick_check")
if check ~= "ok" then die("quick_check=" .. tostring(check)) end
if want == "wal" or want == "delete" then one("PRAGMA journal_mode=" .. want) elseif want ~= "status" then die("usage") end
local mode = one("PRAGMA journal_mode")
io.write("WAL_DB mode=", mode, " check=", check, " version=", ffi.string(lib.sqlite3_libversion()), "\n")
if lib.sqlite3_close(db) ~= 0 then die("close") end
