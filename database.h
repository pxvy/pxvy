#pragma once
#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_DATABASE_H
#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_DATABASE_H
#include<sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "colors.h"
#include <stdbool.h>

char g_recent_video[10][MAX_PATH + 3];
int g_recent_count = 0;

// DB 경로: %USERPROFILE%\.pxvy\settings.db
static char g_db_path[MAX_PATH];

static void build_db_path(void) {
    const char *profile = getenv("USERPROFILE");
    snprintf(g_db_path, MAX_PATH, "%s\\.pxvy\\settings.db", profile ? profile : ".");
}

// .pxvy 폴더 없으면 생성
static void ensure_pxvy_dir(void) {
    const char *profile = getenv("USERPROFILE");
    char dir[MAX_PATH];
    snprintf(dir, MAX_PATH, "%s\\.pxvy", profile ? profile : ".");
    CreateDirectoryA(dir, NULL); // 이미 있으면 무시
}

static void ensure_pxvy_capture_dir(void) {
    char pictures_path[MAX_PATH];
    HRESULT hr = SHGetFolderPathA(NULL, CSIDL_MYPICTURES, NULL, SHGFP_TYPE_CURRENT, pictures_path);
    if (FAILED(hr)) {
        return;
    }

    char capture_dir[MAX_PATH];
    snprintf(capture_dir, MAX_PATH, "%s\\PXVYScreenShots", pictures_path);

    CreateDirectoryA(capture_dir, NULL); // 이미 있으면 무시
}

// ────────────────────────────────────────────
//  DB 초기화: 4개 테이블 생성 + 기본값 삽입
// ────────────────────────────────────────────
static int pxvy_db_init(void) {
    ensure_pxvy_dir();
    ensure_pxvy_capture_dir();
    build_db_path();

    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        say("DB open failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql =
            // 1. ColorTable
            "CREATE TABLE IF NOT EXISTS ColorTable ("
            "  id INTEGER PRIMARY KEY CHECK(id = 1),"
            "  R  INTEGER NOT NULL CHECK(R BETWEEN 0 AND 255),"
            "  G  INTEGER NOT NULL CHECK(G BETWEEN 0 AND 255),"
            "  B  INTEGER NOT NULL CHECK(B BETWEEN 0 AND 255)"
            ");"
            "INSERT OR IGNORE INTO ColorTable(id, R, G, B) VALUES(1, 52, 199, 89);"

            // 2. SubtitleFont
            "CREATE TABLE IF NOT EXISTS SubtitleFont ("
            "  id       INTEGER PRIMARY KEY CHECK(id = 1),"
            "  FontName TEXT NOT NULL"
            ");"
            "INSERT OR IGNORE INTO SubtitleFont(id, FontName) VALUES(1, 'Segoe UI');"

            // 3. CapturePath
            "CREATE TABLE IF NOT EXISTS CapturePath ("
            "  id   INTEGER PRIMARY KEY CHECK(id = 1),"
            "  Path TEXT NOT NULL"
            ");"

            // 4. CaptureFormat
            "CREATE TABLE IF NOT EXISTS CaptureFormat ("
            "  id     INTEGER PRIMARY KEY CHECK(id = 1),"
            "  Format TEXT NOT NULL"
            ");"
            "INSERT OR IGNORE INTO CaptureFormat(id, Format) VALUES(1, 'PNG');" // ← 여기로 이동

            // 5. PlayerVolume
            "CREATE TABLE IF NOT EXISTS PlayerVolume ("
            "  id     INTEGER PRIMARY KEY CHECK(id = 1),"
            "  Volume REAL NOT NULL"
            ");"
            "INSERT OR IGNORE INTO PlayerVolume(id, Volume) VALUES(1, 100.0);" // ← 여기로 이동

            // 6. RecentVideos
            "CREATE TABLE IF NOT EXISTS RecentVideos ("
            "  idx      INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  FilePath TEXT NOT NULL UNIQUE"
            ");";

    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        say("DB init failed: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return -1;
    }

    // CapturePath 기본값만 별도 처리 (바인딩 필요하므로 불가피)
    const char *profile = getenv("USERPROFILE");
    char default_cap[MAX_PATH];
    snprintf(default_cap, MAX_PATH, "%s\\Pictures\\PXVYScreenShots",
             profile ? profile : ".");

    sqlite3_stmt *stmt;
    int rc;

    rc = sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO CapturePath(id, Path) VALUES(1, ?);", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, default_cap, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    } else {
        say("DB CapturePath prepare failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_close(db);
    return 0;
}

static bool pxvy_db_get_capture_directory(char *dir) {
    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        say("DB open failed: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, "SELECT Path FROM CapturePath WHERE id = 1;", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        say("DB CapturePath prepare failed: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    bool success = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *path = sqlite3_column_text(stmt, 0);
        if (path) {
            strncpy(dir, (const char *) path, MAX_PATH - 1);
            dir[MAX_PATH - 1] = '\0';
            success = true;
        }
    } else {
        say("DB CapturePath row not found\n");
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return success;
}

static float pxvy_db_get_volume(void) {
    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        say("DB open failed: %s\n", sqlite3_errmsg(db));
        return 1.0f;
    }

    sqlite3_stmt *stmt;
    float result = 1.0f;

    if (sqlite3_prepare_v2(db, "SELECT Volume FROM PlayerVolume WHERE id = 1;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW)
            result = (float) sqlite3_column_double(stmt, 0);
        sqlite3_finalize(stmt);
    } else {
        say("DB get_volume prepare failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_close(db);
    return result;
}

static bool pxvy_db_set_volume(float volume) {
    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        say("DB open failed: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_stmt *stmt;
    bool success = false;

    if (sqlite3_prepare_v2(db, "UPDATE PlayerVolume SET Volume = ? WHERE id = 1;", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, (double) volume);
        if (sqlite3_step(stmt) == SQLITE_DONE)
            success = sqlite3_changes(db) > 0;
        else
            say("DB set_volume step failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
    } else {
        say("DB set_volume prepare failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_close(db);
    return success;
}

static bool pxvy_db_set_capture_type(char *image_format) {
    if (strcmp(image_format, "PNG") != 0 && strcmp(image_format, "JPG") != 0 && strcmp(image_format, "WEBP") != 0) {
        return false;
    }


    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        say("DB open failed: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_stmt *stmt;
    bool success = false;

    if (sqlite3_prepare_v2(db, "UPDATE CaptureFormat SET Format = ? WHERE id = 1;", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, image_format, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE)
            success = sqlite3_changes(db) > 0;
        else
            say("DB set_capture_type step failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
    } else {
        say("DB set_capture_type prepare failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_close(db);
    return success;
}

static int pxvy_db_get_capture_type(void) {
    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        say("DB open failed: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_stmt *stmt;
    int result = 0;

    if (sqlite3_prepare_v2(db, "SELECT Format FROM CaptureFormat WHERE id = 1;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *fmt = (const char *) sqlite3_column_text(stmt, 0);
            if (fmt) {
                if (strcmp(fmt, "PNG") == 0) result = 1;
                else if (strcmp(fmt, "JPG") == 0) result = 2;
                else if (strcmp(fmt, "WEBP") == 0) result = 3;
            }
        }
        sqlite3_finalize(stmt);
    } else {
        say("DB get_capture_type prepare failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_close(db);
    return result;
}

static bool pxvy_db_get_capture_path(char *path) {
    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        say("DB open failed: %s\n", sqlite3_errmsg(db));
        return false;
    }

    const char *sql = "SELECT Path FROM CapturePath WHERE id = 1;";
    sqlite3_stmt *stmt;
    bool success = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *val = (const char *) sqlite3_column_text(stmt, 0);
            if (val) {
                strncpy(path, val, MAX_PATH - 1);
                path[MAX_PATH - 1] = '\0';
                success = true;
            }
        }
        sqlite3_finalize(stmt);
    } else {
        say("DB get_capture_path prepare failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_close(db);
    return success;
}

static bool pxvy_db_set_capture_path(char *path) {
    build_db_path();

    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        say("DB open failed: %s\n", sqlite3_errmsg(db));
        return false;
    }

    const char *sql = "UPDATE CapturePath SET Path = ? WHERE id = 1;";
    sqlite3_stmt *stmt;
    bool success = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, path, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            success = sqlite3_changes(db) > 0;
        } else {
            say("DB set_capture_path step failed: %s\n", sqlite3_errmsg(db));
        }
        sqlite3_finalize(stmt);
    } else {
        say("DB set_capture_path prepare failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_close(db);
    return success;
}


static int pxvy_clear_recent_video(void) {
    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) return -1;
    sqlite3_exec(db, "DELETE FROM RecentVideos;", NULL, NULL, NULL);
    sqlite3_close(db);
    return 0;
}

static int pxvy_add_recent_video(const char *file_path) {
    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) return -1;

    sqlite3_stmt *stmt;
    int rc = -1;

    // 중복이면 삭제 후 재삽입 → idx가 새로 발급되어 최신 순위로 올라옴 
    const char *del_sql = "DELETE FROM RecentVideos WHERE FilePath = ?; ";
    if (sqlite3_prepare_v2(db, del_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, file_path, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    const char *ins_sql = "INSERT INTO RecentVideos(FilePath) VALUES(?); ";
    if (sqlite3_prepare_v2(db, ins_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, file_path, -1, SQLITE_STATIC);
        rc = (sqlite3_step(stmt) == SQLITE_DONE) ? 0 : -1;
        sqlite3_finalize(stmt);
    }

    // 10개 초과분(오래된 것) 제거 
    const char *trim_sql =
            "DELETE FROM RecentVideos WHERE idx NOT IN"
            "  (SELECT idx FROM RecentVideos ORDER BY idx DESC LIMIT 10); ";
    sqlite3_exec(db, trim_sql, NULL, NULL, NULL);

    sqlite3_close(db);
    return rc;
}


static int pxvy_get_recent_video(void) {
    g_recent_count = 0;
    memset(g_recent_video, 0, sizeof(g_recent_video));

    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) return -1;

    const char *sql =
            "SELECT FilePath FROM RecentVideos ORDER BY idx DESC LIMIT 10;";
    sqlite3_stmt *stmt;
    int rc = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW && g_recent_count < 10) {
            const char *path = (const char *) sqlite3_column_text(stmt, 0);
            if (path) {
                strncpy(g_recent_video[g_recent_count], path, MAX_PATH + 2);
                g_recent_video[g_recent_count][MAX_PATH + 2] = '\0';
                g_recent_count++;
            }
        }
        sqlite3_finalize(stmt);
        rc = 0;
    }

    sqlite3_close(db);
    return rc;
}

static int pxvy_remove_recent_video(const char *file_path) {
    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) return -1;
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM RecentVideos WHERE FilePath = ?;";
    int rc = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, file_path, -1, SQLITE_STATIC);
        rc = (sqlite3_step(stmt) == SQLITE_DONE) ? 0 : -1;
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    return rc;
}

// ────────────────────────────────────────────
//  FontName 가져오기
//  buf: 호출자가 준비한 버퍼, buf_size: 크기
//  반환값: 0 성공, -1 실패
// ────────────────────────────────────────────
// ────────────────────────────────────────────
//  SubtitleFont 저장
//  반환값: 0 성공, -1 실패
// ────────────────────────────────────────────
static int pxvy_set_subtitle_font(const char *font_name) {
    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) return -1;

    const char *sql =
            "INSERT INTO SubtitleFont(id, FontName) VALUES(1, ?) "
            " ON CONFLICT(id) DO UPDATE SET FontName = excluded.FontName; ";

    sqlite3_stmt *stmt;
    int rc = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, font_name, -1, SQLITE_STATIC);
        rc = (sqlite3_step(stmt) == SQLITE_DONE) ? 0 : -1;
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return rc;
}

// ────────────────────────────────────────────
//  SubtitleFont 로드
//  buf: 호출자가 준비한 버퍼, buf_size: 크기
//  반환값: 0 성공, -1 실패 (buf 는 빈 문자열로 초기화)
// ────────────────────────────────────────────
static int pxvy_get_subtitle_font(char *buf, int buf_size) {
    if (!buf || buf_size <= 0) return -1;
    buf[0] = '\0';

    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) return -1;

    sqlite3_stmt *stmt;
    const char *sql = "SELECT FontName FROM SubtitleFont WHERE id = 1;";
    int rc = -1;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *val = (const char *) sqlite3_column_text(stmt, 0);
            strncpy(buf, val ? val : "", buf_size - 1);
            buf[buf_size - 1] = '\0';
            rc = 0;
        }
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return rc;
}

static Color3 pxvy_get_color() {
    sqlite3 *db;
    Color3 c = {52, 199, 89};
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        return c;
    }
    int r, g, b;
    sqlite3_stmt *stmt;
    const char *sql = "SELECT R, G, B FROM ColorTable WHERE id = 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            r = sqlite3_column_int(stmt, 0);
            g = sqlite3_column_int(stmt, 1);
            b = sqlite3_column_int(stmt, 2);
        }
        sqlite3_finalize(stmt);
    }
    printf("DB: %d, %d. %d\n", r, g, b);
    sqlite3_close(db);
    c.r = r;
    c.g = g;
    c.b = b;
    return c;
}

static int pxvy_set_color(int r, int g, int b) {
    sqlite3 *db;
    say("pxvy set color");
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) return -1;

    const char *sql =
            "INSERT INTO ColorTable(id, R, G, B) VALUES(1, ?, ?, ?) " // ← 끝에 공백
            "ON CONFLICT(id) DO UPDATE SET " // ← 끝에 공백
            "  R = excluded.R,"
            "  G = excluded.G,"
            "  B = excluded.B;";

    sqlite3_stmt *stmt;
    int rc = -1;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, r);
        sqlite3_bind_int(stmt, 2, g);
        sqlite3_bind_int(stmt, 3, b);
        rc = (sqlite3_step(stmt) == SQLITE_DONE) ? 0 : -1;
        if (rc != 0)
            say("pxvy_set_color step failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
    } else {
        say("pxvy_set_color prepare failed: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_close(db);
    return rc;
}


#endif //RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_DATABASE_H
