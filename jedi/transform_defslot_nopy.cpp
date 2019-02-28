#include <string>
#include <tuple>
#include <cctype>
#include <algorithm>
#include <string_view>

namespace transform {

static const std::string DEFSLOT("defslot(");
static const std::string DEFCONST("defconst(");
static const std::string DEFVIRTSLOT("defvirtslot(");
static const std::string TYPE_STMT("type=");

enum class StatementType {
    defslot,
    defvirtslot,
    defconst
};

auto find_end(const std::string_view source, size_t start) {
    auto num_newlines = 0;
    auto num_parens = 1;

    auto it = begin(source) + start;
    auto end_iter = end(source);
    while (it != end_iter && num_parens > 0) {
        switch(*it++) {
            case '\n':
                ++num_newlines;
                break;
            case '(':
                ++num_parens;
                break;
            case ')':
                --num_parens;
                break;
        }
    }

    return std::make_tuple(num_newlines, it - begin(source));
}

template<typename Iterator>
auto extract_type(Iterator type_begin, Iterator stmt_end) {
    auto type_end = type_begin;
    while (type_end != stmt_end && *type_end != ',' && *type_end != ')') {
        ++type_end;
    }
    return std::make_tuple(type_begin, type_end);
}

template <typename Iterator>
void append_init(Iterator stmt_begin, Iterator stmt_end, std::string& result) {
    auto type_start = std::search(stmt_begin, stmt_end, begin(TYPE_STMT), end(TYPE_STMT));
    if (type_start == stmt_end) {
        result.append(" = None");
        return;
    }

    auto [it1, it2] = extract_type(type_start + TYPE_STMT.size(), stmt_end);
    result.append(" = ");
    result.append(it1, it2);
    result.append("()");
}

auto transform_single(const std::string_view source, size_t start,
                      StatementType stmt_type, size_t& pos) {
    static size_t statement_lengths[] = {
        DEFSLOT.size(),
        DEFVIRTSLOT.size(),
        DEFCONST.size(),
    };
    size_t defstmt_len = statement_lengths[static_cast<int>(stmt_type)];

    size_t num_newlines;
    size_t stmt_end;
    std::tie(num_newlines, stmt_end) = find_end(source, start + defstmt_len);

    std::string result;
    auto it = begin(source) + start + defstmt_len;
    // Skip spaces between ( and quote
    while (isspace(*it))
        ++it;

    char delimiter;
    if (*it == '\'') {
        delimiter = '\'';
    } else if (*it == '"') {
        delimiter = '"';
    } else {
        // Something is wrong, don't try to parse the statement.
        result.append(begin(source) + start, begin(source) + stmt_end);
        pos = stmt_end;
        return result;
    }
    ++it;

    auto name_iter = std::find(it, end(source), delimiter);
    if (name_iter == end(source)) { // unclosed quote
        result.append(begin(source) + start, begin(source) + stmt_end);
        pos = stmt_end;
        return result;
    }

    result.append(it, name_iter);
    if (stmt_type == StatementType::defvirtslot)
        result.append(" = None");
    else
        append_init(name_iter, begin(source) + stmt_end, result);
    result.append(num_newlines, '\n');

    pos = stmt_end;
    return result;
}

std::string transform_source(const std::string_view source) {
    std::string result;
    result.reserve(source.size());

    auto begin_iter = begin(source);
    auto remaining = begin_iter;
    size_t pos = 0;
    while (true) {
        StatementType stmt_type = StatementType::defslot;
        auto start = source.find(DEFSLOT, pos);
        auto defconst_start = source.find(DEFCONST, pos);
        if (start != std::string::npos && defconst_start != std::string::npos) {
            if (defconst_start < start) {
                start = defconst_start;
                stmt_type = StatementType::defconst;
            }
        } else {
            if (start == std::string::npos) {
                stmt_type = StatementType::defconst;
                start = defconst_start;
            }
        }
        if (start == std::string::npos) {
            result.append(remaining, end(source));
            break;
        }

        result.append(remaining, begin_iter + start);
        result.append(transform_single(source, start, stmt_type, pos));
        remaining = begin_iter + pos;
    }

    return result;
}

}

extern "C" {

char *transform_source(const char *source, unsigned long length) {
    std::string result = transform::transform_source(std::string_view(source, length));
    char *transformed = new char[result.size() + 1];
    result.copy(transformed, result.size());
    transformed[result.size()] = '\0';
    return transformed;
}

void free_transformed(char *transformed) {
    delete[] transformed;
}

}
