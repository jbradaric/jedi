#include <string>
#include <tuple>
#include <cctype>
#include <optional>
#include <algorithm>

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

typedef std::tuple<std::string::const_iterator, std::string::const_iterator> iter_tuple;

std::tuple<size_t, size_t> find_end(const std::string& source, size_t start) {
    size_t num_newlines = 0;
    size_t num_parens = 1;

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

    return std::make_tuple(num_newlines,
                           it - begin(source));
}

iter_tuple extract_type(std::string::const_iterator type_begin,
                        std::string::const_iterator stmt_end) {
    auto type_end = type_begin;
    while (type_end != stmt_end
           && *type_end != ','
           && *type_end != ')') {
        ++type_end;
    }
    return std::make_tuple(type_begin, type_end);
}

void append_init(std::string::const_iterator stmt_begin,
                 std::string::const_iterator stmt_end,
                 StatementType stmt_type,
                 std::string& result) {
    if (stmt_type == StatementType::defvirtslot) {
        result.append(" = None");
        return;
    }

    auto type_start = std::search(stmt_begin, stmt_end, begin(TYPE_STMT), end(TYPE_STMT));
    if (type_start == stmt_end) {
        result.append(" = None");
        return;
    }

    std::string::const_iterator it1, it2;
    std::tie(it1, it2) = extract_type(type_start + TYPE_STMT.size(), stmt_end);
    result.append(" = ");
    result.append(it1, it2);
    result.append("()");
}

std::string transform_single(const std::string& source, size_t start,
                             StatementType stmt_type, size_t& pos) {
    size_t defstmt_len = 0;
    if (stmt_type == StatementType::defslot) {
        defstmt_len = DEFSLOT.size();
    } else if (stmt_type == StatementType::defvirtslot) {
        defstmt_len = DEFVIRTSLOT.size();
    } else {
        defstmt_len = DEFCONST.size();
    }

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
    append_init(name_iter, begin(source) + stmt_end, stmt_type, result);
    result.append(num_newlines, '\n');

    pos = stmt_end;

    return result;
}

std::optional<std::tuple<StatementType, size_t>>
find_first_attrdef(const std::string& source, size_t pos) {
    auto defslot_start = source.find(DEFSLOT, pos);
    auto defvirtslot_start = source.find(DEFVIRTSLOT, pos);
    auto defconst_start = source.find(DEFCONST, pos);
    auto values = {
        std::make_tuple(StatementType::defslot, defslot_start),
        std::make_tuple(StatementType::defvirtslot, defvirtslot_start),
        std::make_tuple(StatementType::defconst, defconst_start),
    };

    auto min_elem = std::min_element(
            begin(values), end(values),
            [] (auto const& a, auto const& b) -> bool {
            using std::get;
            if (get<1>(a) == std::string::npos && get<1>(b) == std::string::npos)
                return false;
            else if (get<1>(a) == std::string::npos)
                return false;
            else if (get<1>(b) == std::string::npos)
                return true;
            else
                return false;
            });
    if (std::get<1>(*min_elem) == std::string::npos)
        return std::nullopt;
    return std::make_optional(*min_elem);
}

std::string transform_source(const std::string& source) {
    std::string result;
    result.reserve(source.size());

    auto begin_iter = begin(source);
    auto remaining = begin_iter;
    size_t pos = 0;
    while (true) {
        auto first_attrdef = find_first_attrdef(source, pos);
        if (!first_attrdef) {
            result.append(remaining, end(source));
            break;
        }

        auto stmt_type = std::get<0>(*first_attrdef);
        auto start = std::get<1>(*first_attrdef);
        result.append(remaining, begin_iter + start);
        result.append(transform_single(source, start, stmt_type, pos));
        remaining = begin_iter + pos;
    }

    return result;
}

}

extern "C" {

char *transform_source(const char *source, unsigned long length) {
    std::string result = transform::transform_source(std::string(source, length));
    char *transformed = new char[result.size() + 1];
    result.copy(transformed, result.size());
    transformed[result.size()] = '\0';
    return transformed;
}

void free_transformed(char *transformed) {
    delete[] transformed;
}

}
