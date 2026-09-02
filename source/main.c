#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include "icon0_png.h"

/* Rebrands NPXS40047 (PlayStation Store) into Jailbreak Store. */

#define APP_DB_PATH      "/system_data/priv/mms/app.db"
#define APPINFO_DB_PATH  "/system_data/priv/mms/appinfo.db"

#define TITLE_ID      "NPXS40047"
#define NEW_TITLE     "Jailbreak Store"
#define NEW_DEEPLINK  "https://vinasxexplosao.github.io/slopkitNadarepublicano/"
#define ICON_DIR      "/user/appmeta/" TITLE_ID
#define ICON_PATH     ICON_DIR "/icon0.png"
#define NEW_ICON0     ICON_PATH "?ts=1"

#if !defined(dstr)
#define dstr(s) #s
#endif
#if !defined(xstr)
#define xstr(s) dstr(s)
#endif

#if !defined(FILE_FUNC_LINE)
#define FILE_FUNC_LINE __FILE__ \
    ":"__FUNCSIG__              \
    ":" xstr(__LINE__)
#endif

#define close_and_exit(db, c)                                   \
    if (db)                                                     \
    {                                                           \
        sqlite3_close(db);                                      \
    }                                                           \
    notify("close_and_exit called from\n" FILE_FUNC_LINE "\n"); \
    return c

#if !defined(_countof)
#define _countof(a) (sizeof(a) / sizeof(*a))
#endif
#if !defined(_countof_1)
#define _countof_1(a) (_countof(a) - 1)
#endif

static void notify(const char* fmt, ...)
{
    struct notify_request
    {
        char useless1[45];
        char message[1024];
        char useless2[2051];
    } buf = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf.message, _countof_1(buf.message), fmt, args);
    va_end(args);
    size_t len = strlen(buf.message);
    while (len > 0 && buf.message[len - 1] == '\n')
    {
        buf.message[--len] = '\0';
    }
    extern int sceKernelSendNotificationRequest(const size_t, const struct notify_request*, const size_t, const int);
    puts(buf.message);
    sceKernelSendNotificationRequest(0, &buf, sizeof(buf), 0);
}

static int write_icon_file(void)
{
    if (mkdir(ICON_DIR, 0777) != 0 && errno != EEXIST)
    {
        return -1;
    }
    FILE* out = fopen(ICON_PATH, "wb");
    if (!out)
    {
        return -1;
    }
    size_t written = fwrite(g_icon0_png, 1, g_icon0_png_len, out);
    int close_error = fclose(out);
    return (written != g_icon0_png_len || close_error) ? -1 : 0;
}

/* Binds params[0..nparams) as TEXT in order, runs sql, returns rows changed or -1 on error. */
static int exec_update(sqlite3* db, const char* sql, const char** params, int nparams)
{
    sqlite3_stmt* stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        fprintf(stderr, "Prepare error [%s]: %s\n", sql, sqlite3_errmsg(db));
        return -1;
    }
    for (int i = 0; i < nparams; i++)
    {
        sqlite3_bind_text(stmt, i + 1, params[i], -1, SQLITE_STATIC);
    }
    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        fprintf(stderr, "Step error [%s]: %s\n", sql, sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    int changes = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    return changes;
}

static int lookup_local_concept_id(sqlite3* app_db, char* out, size_t out_sz)
{
    sqlite3_stmt* stmt = NULL;
    int found = 0;
    if (sqlite3_prepare_v2(app_db, "SELECT localConceptId FROM tbl_contentinfo WHERE titleId = ?", -1, &stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    sqlite3_bind_text(stmt, 1, TITLE_ID, -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* v = sqlite3_column_text(stmt, 0);
        if (v)
        {
            strncpy(out, (const char*)v, out_sz - 1);
            out[out_sz - 1] = '\0';
            found = 1;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

/* appinfo.db has no DEEPLINK_URI row yet, so insert if missing, update if a prior run added it. */
static int upsert_appinfo_deeplink(sqlite3* appinfo_db)
{
    sqlite3_stmt* stmt = NULL;
    int had_row = 0;
    if (sqlite3_prepare_v2(appinfo_db, "SELECT 1 FROM tbl_appinfo WHERE titleId = ? AND key = 'DEEPLINK_URI'", -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, TITLE_ID, -1, SQLITE_STATIC);
        had_row = (sqlite3_step(stmt) == SQLITE_ROW);
        sqlite3_finalize(stmt);
    }

    if (had_row)
    {
        const char* params[] = {NEW_DEEPLINK, TITLE_ID};
        return exec_update(appinfo_db, "UPDATE tbl_appinfo SET val = ? WHERE titleId = ? AND key = 'DEEPLINK_URI'", params, 2);
    }

    char meta_data_id[64] = "prior";
    if (sqlite3_prepare_v2(appinfo_db, "SELECT metaDataId FROM tbl_appinfo WHERE titleId = ? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK)
    {
        sqlite3_bind_text(stmt, 1, TITLE_ID, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char* m = sqlite3_column_text(stmt, 0);
            if (m)
            {
                strncpy(meta_data_id, (const char*)m, sizeof(meta_data_id) - 1);
            }
        }
        sqlite3_finalize(stmt);
    }

    const char* params[] = {TITLE_ID, meta_data_id, NEW_DEEPLINK};
    return exec_update(appinfo_db, "INSERT INTO tbl_appinfo (titleId, metaDataId, key, val) VALUES (?, ?, 'DEEPLINK_URI', ?)", params, 3);
}

/* Rebrands titleName/conceptName/icon0Info in every per-account app.db table. */
static int rebrand_per_account_tables(sqlite3* app_db, const char* local_concept_id)
{
    sqlite3_stmt* stmt = NULL;
    int total = 0;
    if (sqlite3_prepare_v2(app_db, "SELECT name FROM sqlite_master WHERE type = 'table'", -1, &stmt, NULL) != SQLITE_OK)
    {
        return -1;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const char* name = (const char*)sqlite3_column_text(stmt, 0);
        if (!name)
        {
            continue;
        }

        char sql[256];
        int changes;

        if (!strncmp(name, "tbl_iconinfo_", 13))
        {
            const char* params[] = {NEW_TITLE, TITLE_ID};
            snprintf(sql, sizeof(sql), "UPDATE %s SET titleName = ? WHERE titleId = ?", name);
            changes = exec_update(app_db, sql, params, 2);
        }
        else if (!strncmp(name, "tbl_concepticoninfo_", 20))
        {
            const char* params[] = {NEW_TITLE, NEW_TITLE, NEW_ICON0, local_concept_id};
            snprintf(sql, sizeof(sql), "UPDATE %s SET conceptName = ?, primaryTitleName = ?, icon0Info = ? WHERE localConceptId = ?", name);
            changes = exec_update(app_db, sql, params, 4);
        }
        else
        {
            continue;
        }

        if (changes > 0)
        {
            printf("  %-32s rows updated: %d\n", name, changes);
            total += changes;
        }
    }

    sqlite3_finalize(stmt);
    return total;
}

int main(void)
{
    FILE* f = fopen(APP_DB_PATH, "r");
    if (!f)
    {
        fprintf(stderr, "Error: %s file not found!\n", APP_DB_PATH);
        return 1;
    }
    fclose(f);

    f = fopen(APPINFO_DB_PATH, "r");
    if (!f)
    {
        fprintf(stderr, "Error: %s file not found!\n", APPINFO_DB_PATH);
        return 1;
    }
    fclose(f);

    if (write_icon_file())
    {
        fprintf(stderr, "Error writing %s: %s\n", ICON_PATH, strerror(errno));
        return 1;
    }

    notify("Rebranding " TITLE_ID " to \"" NEW_TITLE "\"...\n");

    /* ---------------- app.db ---------------- */
    sqlite3* db = NULL;
    int rc = sqlite3_open(APP_DB_PATH, &db);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Error opening DB: %s\n", sqlite3_errmsg(db));
        close_and_exit(db, 1);
    }

    char local_concept_id[128] = {0};
    if (!lookup_local_concept_id(db, local_concept_id, sizeof(local_concept_id)))
    {
        fprintf(stderr, "Error: " TITLE_ID " has no tbl_contentinfo row\n");
        close_and_exit(db, 1);
    }

    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    int total = 0;
    int changes;
    int content_changes = 0;

    {
        const char* params[] = {NEW_DEEPLINK, NEW_DEEPLINK, TITLE_ID};
        changes = exec_update(db,
            "UPDATE tbl_contentinfo SET pprDeeplinkUri = ?, "
            "AppInfoJson = json_set(AppInfoJson, '$.DEEPLINK_URI', ?) WHERE titleId = ?",
            params, 3);
        if (changes < 0)
        {
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            close_and_exit(db, 1);
        }
        content_changes += changes;
    }
    {
        const char* params[] = {NEW_TITLE, NEW_TITLE, NEW_TITLE, TITLE_ID};
        changes = exec_update(db,
            "UPDATE tbl_contentinfo SET titleName = ?, "
            "AppInfoJson = json_set(json_set(AppInfoJson, '$.TITLE', ?), '$.TITLE_01', ?) WHERE titleId = ?",
            params, 4);
        if (changes < 0)
        {
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            close_and_exit(db, 1);
        }
        content_changes += changes;
    }
    {
        const char* params[] = {NEW_ICON0, NEW_ICON0, TITLE_ID};
        changes = exec_update(db,
            "UPDATE tbl_contentinfo SET icon0Info = ?, "
            "AppInfoJson = json_set(AppInfoJson, '$.icon0Info', ?) WHERE titleId = ?",
            params, 3);
        if (changes < 0)
        {
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            close_and_exit(db, 1);
        }
        content_changes += changes;
    }
    printf("  tbl_contentinfo rows updated: %d\n", content_changes);
    total += content_changes;

    int per_account = rebrand_per_account_tables(db, local_concept_id);
    if (per_account < 0)
    {
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        close_and_exit(db, 1);
    }
    total += per_account;

    {
        const char* params[] = {NEW_TITLE, NEW_ICON0, local_concept_id};
        changes = exec_update(db, "UPDATE tbl_conceptmetadata SET conceptName = ?, icon0Info = ? WHERE localConceptId = ?", params, 3);
        if (changes > 0)
        {
            printf("  tbl_conceptmetadata rows updated: %d\n", changes);
            total += changes;
        }
    }

    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_close(db);
    db = NULL;

    /* ---------------- appinfo.db ---------------- */
    rc = sqlite3_open(APPINFO_DB_PATH, &db);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Error opening DB: %s\n", sqlite3_errmsg(db));
        close_and_exit(db, 1);
    }

    sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL);

    changes = upsert_appinfo_deeplink(db);
    if (changes > 0)
    {
        total += changes;
    }

    {
        const char* params[] = {NEW_TITLE, TITLE_ID};
        changes = exec_update(db, "UPDATE tbl_appinfo SET val = ? WHERE titleId = ? AND key = 'TITLE'", params, 2);
        if (changes > 0) total += changes;
        changes = exec_update(db, "UPDATE tbl_appinfo SET val = ? WHERE titleId = ? AND key = 'TITLE_01'", params, 2);
        if (changes > 0) total += changes;
    }
    {
        const char* params[] = {NEW_TITLE, local_concept_id};
        changes = exec_update(db, "UPDATE tbl_conceptinfo SET val = ? WHERE local_concept_id = ? AND key = 'concept_name'", params, 2);
        if (changes > 0) total += changes;
        changes = exec_update(db, "UPDATE tbl_conceptinfo SET val = ? WHERE local_concept_id = ? AND key = 'concept_name_01'", params, 2);
        if (changes > 0) total += changes;
    }

    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_close(db);
    db = NULL;

    notify("Done. " TITLE_ID " is now \"" NEW_TITLE "\" (%d rows).", total);
    notify("Reboot PS5 to see the changes.\nmade by maj0r 💙");
    return 0;
}
