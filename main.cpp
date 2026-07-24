#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "optimizer.h"
#include "executor.h"

int main() {
    // ── In-memory table with test data ───────────────────────────
    mydb::TableStorage storage;
    storage["users"] = {
        {{"id","1"}, {"name","Alice"}, {"age","35"}},
        {{"id","2"}, {"name","Bob"},   {"age","22"}},
        {{"id","3"}, {"name","Carol"}, {"age","31"}},
        {{"id","4"}, {"name","Dave"},  {"age","17"}},
        {{"id","5"}, {"name","Eve"},   {"age","45"}},
    };

    std::string sql =
        "SELECT id, name FROM users WHERE age >= 30 LIMIT 3;";

    std::cout << "SQL: " << sql << "\n\n";

    // ── Pipeline: Lex → Parse → Optimize → Execute ───────────────
    mydb::Lexer     lexer(sql);
    mydb::Parser    parser(lexer.tokenize());
    mydb::Optimizer optimizer;
    mydb::Executor  executor(storage);

    auto stmt     = parser.parse();
    auto logical  = optimizer.toLogical(stmt);
    auto physical = optimizer.toPhysical(logical);
    auto results  = executor.execute(physical);

    // ── Print results ─────────────────────────────────────────────
    std::cout << "Results (" << results.size() << " rows):\n";
    std::cout << "─────────────────────\n";
    for (const auto& row : results) {
        for (const auto& [col, val] : row)
            std::cout << col << ": " << val << "  ";
        std::cout << "\n";
    }

    return 0;
}