#pragma once
#ifndef RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_DATABASE_H
#define RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_DATABASE_H
#include<sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "colors.h"
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

// ────────────────────────────────────────────
//  DB 초기화: 4개 테이블 생성 + 기본값 삽입
// ────────────────────────────────────────────
static int pxvy_db_init(void) {
    ensure_pxvy_dir();
    build_db_path();

    sqlite3 *db;
    if (sqlite3_open(g_db_path, &db) != SQLITE_OK) {
        fprintf(stderr, "DB open failed: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    const char *sql =
            /* 1. ColorTable */
            "CREATE TABLE IF NOT EXISTS ColorTable ("
            "  id    INTEGER PRIMARY KEY CHECK(id = 1),"
            "  R     INTEGER NOT NULL CHECK(R BETWEEN 1 AND 255),"
            "  G     INTEGER NOT NULL CHECK(G BETWEEN 1 AND 255),"
            "  B     INTEGER NOT NULL CHECK(B BETWEEN 1 AND 255)"
            ");"
            "INSERT OR IGNORE INTO ColorTable(id, R, G, B) VALUES(1, 52, 199, 89);"

            /* 2. SubtitleFont */
            "CREATE TABLE IF NOT EXISTS SubtitleFont ("
            "  id       INTEGER PRIMARY KEY CHECK(id = 1),"
            "  FontName TEXT NOT NULL"
            ");"
            "INSERT OR IGNORE INTO SubtitleFont(id, FontName) VALUES(1, 'Segoe UI');"

            /* 3. CapturePath */
            "CREATE TABLE IF NOT EXISTS CapturePath ("
            "  id   INTEGER PRIMARY KEY CHECK(id = 1),"
            "  Path TEXT NOT NULL"
            ");"

            /* 4. RecentVideos */
            "CREATE TABLE IF NOT EXISTS RecentVideos ("
            "  idx      INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  FilePath TEXT NOT NULL UNIQUE"
            ");";

    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        fprintf(stderr, "DB init failed: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return -1;
    }

    // CapturePath 기본값: %USERPROFILE%\Pictures\PXVYScreenShots
    // (환경변수 展開 후 삽입)
    const char *profile = getenv("USERPROFILE");
    char default_cap[MAX_PATH];
    snprintf(default_cap, MAX_PATH,
             "%s\\Pictures\\PXVYScreenShots",
             profile ? profile : ".");

    const char *cap_sql =
            "INSERT OR IGNORE INTO CapturePath(id, Path) VALUES(1, ?);";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, cap_sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, default_cap, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_close(db);
    return 0;
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

    /* 중복이면 삭제 후 재삽입 → idx가 새로 발급되어 최신 순위로 올라옴 */
    const char *del_sql = "DELETE FROM RecentVideos WHERE FilePath = ?;";
    if (sqlite3_prepare_v2(db, del_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, file_path, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    const char *ins_sql = "INSERT INTO RecentVideos(FilePath) VALUES(?);";
    if (sqlite3_prepare_v2(db, ins_sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, file_path, -1, SQLITE_STATIC);
        rc = (sqlite3_step(stmt) == SQLITE_DONE) ? 0 : -1;
        sqlite3_finalize(stmt);
    }

    /* 10개 초과분(오래된 것) 제거 */
    const char *trim_sql =
            "DELETE FROM RecentVideos WHERE idx NOT IN"
            "  (SELECT idx FROM RecentVideos ORDER BY idx DESC LIMIT 10);";
    sqlite3_exec(db, trim_sql, NULL, NULL, NULL);

    sqlite3_close(db);
    return rc;
}

char g_recent_video[10][MAX_PATH + 3];
int g_recent_count = 0;

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
            "INSERT INTO SubtitleFont(id, FontName) VALUES(1, ?)"
            " ON CONFLICT(id) DO UPDATE SET FontName = excluded.FontName;";

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
            "INSERT INTO ColorTable(id, R, G, B) VALUES(1, ?, ?, ?)"
            "ON CONFLICT(id) DO UPDATE SET"
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
        sqlite3_finalize(stmt);
    }

    sqlite3_close(db);
    return rc;
}


#endif //RLAQHADMLGPEJWNDQHRQKDWLAOZMFH_DATABASE_H
