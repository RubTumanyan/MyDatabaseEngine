#include <iostream>
#include "lexer.h"
#include "parser.h"
#include "optimizer.h"

// Prints the physical plan tree with indentation
void printPlan(const mydb::PhysicalPlanRef& node, int depth = 0) {
    if (!node) return;
    std::string pad(depth * 2, ' ');

    switch (node->type) {
    case mydb::PhysicalNodeType::SeqScan: {
        auto* n = static_cast<mydb::PhysicalSeqScan*>(node.get());
        std::cout << pad << "SeqScan(" << n->tableName << ")\n";
        break;
    }
    case mydb::PhysicalNodeType::Filter:
        std::cout << pad << "Filter(WHERE ...)\n";
        break;
    case mydb::PhysicalNodeType::Projection: {
        auto* n = static_cast<mydb::PhysicalProjection*>(node.get());
        std::cout << pad << "Projection(";
        for (auto& c : n->columns) std::cout << c << " ";
        std::cout << ")\n";
        break;
    }
    case mydb::PhysicalNodeType::Sort:
        std::cout << pad << "Sort(ORDER BY ...)\n";
        break;
    case mydb::PhysicalNodeType::Limit: {
        auto* n = static_cast<mydb::PhysicalLimit*>(node.get());
        std::cout << pad << "Limit(" << n->count << ")\n";
        break;
    }
    default: break;
    }

    for (auto& child : node->children)
        printPlan(child, depth + 1);
}

int main() {
    std::string sql =
        "SELECT id, name FROM users WHERE age >= 30 ORDER BY name ASC LIMIT 10;";

    std::cout << "SQL: " << sql << "\n\n";

    mydb::Lexer     lexer(sql);
    mydb::Parser    parser(lexer.tokenize());
    mydb::Optimizer optimizer;

    auto stmt     = parser.parse();
    auto logical  = optimizer.toLogical(stmt);
    auto physical = optimizer.toPhysical(logical);

    std::cout << "Physical Plan:\n";
    printPlan(physical);

    return 0;
}