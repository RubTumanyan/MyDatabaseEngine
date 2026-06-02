#include <iostream>
#include "lexer.h"

int main() {
    std::string sql =
        "SELECT id, name FROM users WHERE age >= 30 AND name != 'Bob';";

    std::cout << "Input: " << sql << "\n\n";

    mydb::Lexer lexer(sql);
    auto tokens = lexer.tokenize();

    for (const auto& tok : tokens) {
        std::cout << "  [" << mydb::tokenTypeToString(tok.type) << "]";
        if (!tok.value.empty())
            std::cout << " \"" << tok.value << "\"";
        std::cout << "  (line " << tok.line << ", col " << tok.col << ")\n";
    }

    return 0;
}