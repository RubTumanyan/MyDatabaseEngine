#include <iostream>
#include "include/lexer.h"
#include "include/parser.h"
#include "include/ast.h"

// НЕ пишем using namespace mydb здесь — конфликтует с std::variant

void printExpr(const mydb::Expr& expr, int indent = 0) {
    std::string pad(indent * 2, ' ');

    std::visit([&](auto&& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, std::shared_ptr<mydb::LiteralExpr>>) {
            std::cout << pad << "Literal(" << e->value << ")\n";

        } else if constexpr (std::is_same_v<T, std::shared_ptr<mydb::ColumnRefExpr>>) {
            std::cout << pad << "Column(" << e->column << ")\n";

        } else if constexpr (std::is_same_v<T, std::shared_ptr<mydb::BinaryExpr>>) {
            std::cout << pad << "BinaryExpr(" << e->op << ")\n";
            printExpr(e->left,  indent + 1);
            printExpr(e->right, indent + 1);

        } else if constexpr (std::is_same_v<T, std::shared_ptr<mydb::UnaryExpr>>) {
            std::cout << pad << "UnaryExpr(" << e->op << ")\n";
            printExpr(e->operand, indent + 1);
        }

    }, expr);
}

int main() {
    std::string sql =
        "SELECT id, name FROM users WHERE age >= 30 AND name != 'Bob' LIMIT 10;";

    std::cout << "SQL: " << sql << "\n\n";

    mydb::Lexer  lexer(sql);
    mydb::Parser parser(lexer.tokenize());
    auto         stmt = parser.parse();

    auto& sel = std::get<mydb::SelectStatement>(stmt);

    std::cout << "Statement : SELECT\n";
    std::cout << "Table     : " << sel.table << "\n";

    std::cout << "Columns   : ";
    for (auto& c : sel.columns) std::cout << c << " ";
    std::cout << "\n";

    if (sel.limit)
        std::cout << "Limit     : " << *sel.limit << "\n";

    if (sel.where) {
        std::cout << "\nWHERE AST:\n";
        printExpr(*sel.where, 1);
    }

    return 0;
}