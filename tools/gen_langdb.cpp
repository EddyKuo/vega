// Build-time tool: generate language.db from SQL seed file
// Usage: gen_langdb <seed.sql> <output.db>

#include <sqlite3.h>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

int main(int argc, char* argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <seed.sql> <output.db>\n", argv[0]);
        return 1;
    }

    const char* sql_path = argv[1];
    const char* db_path  = argv[2];

    // Read SQL file
    std::ifstream file(sql_path);
    if (!file.is_open()) {
        fprintf(stderr, "Error: cannot open %s\n", sql_path);
        return 1;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string sql = ss.str();

    // Remove old DB if exists
    std::filesystem::remove(db_path);

    // Create and populate DB
    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error: sqlite3_open failed: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    char* errmsg = nullptr;
    rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error: SQL execution failed: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
        sqlite3_close(db);
        return 1;
    }

    // Report stats
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM translations", -1, &stmt, nullptr);
    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM languages", -1, &stmt, nullptr);
    int langs = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        langs = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    sqlite3_close(db);

    printf("Generated %s: %d languages, %d translations\n", db_path, langs, count);
    return 0;
}
