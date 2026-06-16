#include <attractor/dot_parser.hpp>
#include <cctype>
#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace attractor {

// -- Comment stripping ---------------------------------------------------------

static auto strip_comments(std::string_view source) -> std::expected<std::string, ParseError>
{
    std::string out;
    out.reserve(source.size());

    std::size_t i = 0;
    const std::size_t n = source.size();
    int line = 1;
    int col = 1;

    auto step = [&]() {
        if (source[i] == '\n') {
            ++line;
            col = 1;
        }
        else {
            ++col;
        }
        ++i;
    };

    while (i < n) {
        if (source[i] == '"') {
            int str_line = line;
            int str_col = col;
            out += source[i];
            step();
            bool closed = false;
            while (i < n) {
                if (source[i] == '\\' && i + 1 < n) {
                    out += source[i];
                    step();
                    out += source[i];
                    step();
                }
                else if (source[i] == '"') {
                    out += source[i];
                    step();
                    closed = true;
                    break;
                }
                else {
                    out += source[i];
                    step();
                }
            }
            if (!closed) {
                return std::unexpected(ParseError{"unterminated string literal", str_line, str_col});
            }
        }
        else if (i + 1 < n && source[i] == '/' && source[i + 1] == '/') {
            while (i < n && source[i] != '\n') {
                out += ' ';
                step();
            }
        }
        else if (i + 1 < n && source[i] == '/' && source[i + 1] == '*') {
            int cm_line = line;
            int cm_col = col;
            out += ' ';
            out += ' ';
            step();
            step();
            bool closed = false;
            while (i < n) {
                if (i + 1 < n && source[i] == '*' && source[i + 1] == '/') {
                    out += ' ';
                    out += ' ';
                    step();
                    step();
                    closed = true;
                    break;
                }
                out += (source[i] == '\n') ? '\n' : ' ';
                step();
            }
            if (!closed) {
                return std::unexpected(ParseError{"unterminated block comment", cm_line, cm_col});
            }
        }
        else {
            out += source[i];
            step();
        }
    }

    return out;
}

// -- Lexer ---------------------------------------------------------------------

enum class TokenKind {
    identifier,
    string_lit,
    integer_lit,
    float_lit,
    bool_lit,
    duration_lit,
    bare_value,
    arrow,
    lbrace,
    rbrace,
    lbracket,
    rbracket,
    equals,
    comma,
    semicolon,
    dash_dash,
    lt_angle,
    eof,
};

struct Token {
    TokenKind kind{TokenKind::eof};
    std::string value;
    int line{1};
    int column{1};
};

struct Lexer {
    std::string src;
    std::size_t pos{0};
    int line{1};
    int col{1};

    explicit Lexer(std::string s) : src(std::move(s)) {}

    void advance()
    {
        if (pos < src.size()) {
            if (src[pos] == '\n') {
                ++line;
                col = 1;
            }
            else {
                ++col;
            }
            ++pos;
        }
    }

    char peek(std::size_t offset = 0) const
    {
        if (pos + offset < src.size()) {
            return src[pos + offset];
        }
        return '\0';
    }

    void skip_whitespace()
    {
        while (pos < src.size() && std::isspace(static_cast<unsigned char>(src[pos]))) {
            advance();
        }
    }

    Token next_token()
    {
        skip_whitespace();
        if (pos >= src.size()) {
            return {TokenKind::eof, "", line, col};
        }

        int tok_line = line;
        int tok_col = col;
        char c = peek();

        // String literal
        if (c == '"') {
            advance();
            std::string val;
            while (pos < src.size() && peek() != '"') {
                if (peek() == '\\' && pos + 1 < src.size()) {
                    advance();
                    char esc = peek();
                    advance();
                    switch (esc) {
                    case 'n':
                        val += '\n';
                        break;
                    case 't':
                        val += '\t';
                        break;
                    case '"':
                        val += '"';
                        break;
                    case '\\':
                        val += '\\';
                        break;
                    default:
                        val += '\\';
                        val += esc;
                        break;
                    }
                }
                else {
                    val += peek();
                    advance();
                }
            }
            if (pos < src.size()) {
                advance();  // closing "
            }
            return {TokenKind::string_lit, std::move(val), tok_line, tok_col};
        }

        // HTML label opening angle bracket (reject)
        if (c == '<') {
            advance();
            return {TokenKind::lt_angle, "<", tok_line, tok_col};
        }

        // Arrow or -- (undirected edge)
        if (c == '-') {
            if (peek(1) == '>') {
                advance();
                advance();
                return {TokenKind::arrow, "->", tok_line, tok_col};
            }
            if (peek(1) == '-') {
                advance();
                advance();
                return {TokenKind::dash_dash, "--", tok_line, tok_col};
            }
            // Negative number
            if (std::isdigit(static_cast<unsigned char>(peek(1)))) {
                std::string val;
                val += c;
                advance();
                while (pos < src.size() && std::isdigit(static_cast<unsigned char>(peek()))) {
                    val += peek();
                    advance();
                }
                if (peek() == '.') {
                    val += '.';
                    advance();
                    while (pos < src.size() && std::isdigit(static_cast<unsigned char>(peek()))) {
                        val += peek();
                        advance();
                    }
                    return {TokenKind::float_lit, std::move(val), tok_line, tok_col};
                }
                return {TokenKind::integer_lit, std::move(val), tok_line, tok_col};
            }
        }

        // Number or duration
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::string val;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(peek()))) {
                val += peek();
                advance();
            }
            // Check for float
            if (peek() == '.') {
                val += '.';
                advance();
                while (pos < src.size() && std::isdigit(static_cast<unsigned char>(peek()))) {
                    val += peek();
                    advance();
                }
                return {TokenKind::float_lit, std::move(val), tok_line, tok_col};
            }
            // Check for duration suffix: ms, s, m, h, d
            // Must not be followed by more alphanumeric after suffix
            auto try_duration = [&]() -> bool {
                std::size_t save_pos = pos;
                int save_line = line;
                int save_col = col;
                std::string suffix;
                // Collect alphabetic suffix
                while (pos < src.size() && std::isalpha(static_cast<unsigned char>(peek()))) {
                    suffix += peek();
                    advance();
                }
                // Check suffix is valid duration unit and next char is not alphanumeric
                bool valid_unit = (suffix == "ms" || suffix == "s" || suffix == "m" || suffix == "h" || suffix == "d");
                bool followed_by_alnum = pos < src.size() && std::isalnum(static_cast<unsigned char>(peek()));
                if (valid_unit && !followed_by_alnum) {
                    val += suffix;
                    return true;
                }
                // Restore position
                pos = save_pos;
                line = save_line;
                col = save_col;
                return false;
            };
            if (try_duration()) {
                return {TokenKind::duration_lit, std::move(val), tok_line, tok_col};
            }
            return {TokenKind::integer_lit, std::move(val), tok_line, tok_col};
        }

        // Identifier, bool_lit, or bare_value
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string val;
            while (pos < src.size()) {
                char ch = peek();
                if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '.' || ch == ':' || ch == '-') {
                    val += ch;
                    advance();
                }
                else {
                    break;
                }
            }
            // bool
            if (val == "true" || val == "false") {
                return {TokenKind::bool_lit, std::move(val), tok_line, tok_col};
            }
            // bare_value if contains extended chars
            bool is_bare = false;
            for (char ch : val) {
                if (ch == '.' || ch == ':' || ch == '-') {
                    is_bare = true;
                    break;
                }
            }
            if (is_bare) {
                return {TokenKind::bare_value, std::move(val), tok_line, tok_col};
            }
            return {TokenKind::identifier, std::move(val), tok_line, tok_col};
        }

        // Single-char tokens
        advance();
        switch (c) {
        case '{':
            return {TokenKind::lbrace, "{", tok_line, tok_col};
        case '}':
            return {TokenKind::rbrace, "}", tok_line, tok_col};
        case '[':
            return {TokenKind::lbracket, "[", tok_line, tok_col};
        case ']':
            return {TokenKind::rbracket, "]", tok_line, tok_col};
        case '=':
            return {TokenKind::equals, "=", tok_line, tok_col};
        case ',':
            return {TokenKind::comma, ",", tok_line, tok_col};
        case ';':
            return {TokenKind::semicolon, ";", tok_line, tok_col};
        default:
            break;
        }

        // Unknown -- skip
        return next_token();
    }

    std::vector<Token> tokenize()
    {
        std::vector<Token> tokens;
        while (true) {
            Token t = next_token();
            tokens.push_back(t);
            if (t.kind == TokenKind::eof) {
                break;
            }
        }
        return tokens;
    }
};

// -- Integer parsing helper ----------------------------------------------------

static auto parse_int_attr(std::string_view s) -> std::optional<int>
{
    if (s.empty()) {
        return std::nullopt;
    }
    int val = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
    if (ec != std::errc{} || ptr != s.data() + s.size()) {
        return std::nullopt;
    }
    return val;
}

// -- Duration and FidelityMode helpers ----------------------------------------

static auto parse_duration(std::string_view s) -> std::optional<TimeoutDuration>
{
    if (s.empty()) {
        return std::nullopt;
    }
    std::size_t i = s.find_first_not_of("0123456789");
    if (i == std::string_view::npos || i == 0) {
        return std::nullopt;
    }

    long long n = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + i, n);
    if (ec != std::errc{}) {
        return std::nullopt;
    }
    if (n <= 0) {
        return std::nullopt;
    }

    std::string_view suffix = s.substr(i);
    std::chrono::milliseconds ms{};
    if (suffix == "ms") {
        ms = std::chrono::milliseconds{n};
    }
    else if (suffix == "s") {
        if (n > std::numeric_limits<long long>::max() / 1000LL) {
            return std::nullopt;
        }
        ms = std::chrono::milliseconds{n * 1000};
    }
    else if (suffix == "m") {
        if (n > std::numeric_limits<long long>::max() / 60'000LL) {
            return std::nullopt;
        }
        ms = std::chrono::milliseconds{n * 60'000};
    }
    else if (suffix == "h") {
        if (n > std::numeric_limits<long long>::max() / 3'600'000LL) {
            return std::nullopt;
        }
        ms = std::chrono::milliseconds{n * 3'600'000};
    }
    else if (suffix == "d") {
        if (n > std::numeric_limits<long long>::max() / 86'400'000LL) {
            return std::nullopt;
        }
        ms = std::chrono::milliseconds{n * 86'400'000};
    }
    else {
        return std::nullopt;
    }

    return TimeoutDuration{ms};
}

static auto parse_reasoning_effort(std::string_view s) -> std::optional<ReasoningEffort>
{
    if (s == "low") {
        return ReasoningEffort::low;
    }
    if (s == "medium") {
        return ReasoningEffort::medium;
    }
    if (s == "high") {
        return ReasoningEffort::high;
    }
    return std::nullopt;
}

static auto parse_fidelity_mode(std::string_view s) -> std::optional<FidelityMode>
{
    if (s == "full") {
        return FidelityMode::full;
    }
    if (s == "truncate") {
        return FidelityMode::truncate;
    }
    if (s == "compact") {
        return FidelityMode::compact;
    }
    if (s == "summary:low") {
        return FidelityMode::summary_low;
    }
    if (s == "summary:medium") {
        return FidelityMode::summary_medium;
    }
    if (s == "summary:high") {
        return FidelityMode::summary_high;
    }
    return std::nullopt;
}

static auto validate_node_id(std::string_view id, int ln, int col_) -> std::optional<ParseError>
{
    if (id.empty()) {
        return ParseError{"empty node ID", ln, col_};
    }
    if (!std::isalpha(static_cast<unsigned char>(id[0])) && id[0] != '_') {
        return ParseError{std::string{"invalid node ID: "} + std::string{id}, ln, col_};
    }
    for (char ch : id.substr(1)) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            return ParseError{std::string{"invalid node ID: "} + std::string{id}, ln, col_};
        }
    }
    return std::nullopt;
}

static auto derive_css_class(std::string_view label) -> std::string
{
    std::string result;
    for (char c : label) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        else if (c == ' ') {
            result += '-';
        }
    }
    // Strip leading/trailing '-'
    while (!result.empty() && result.front() == '-') {
        result.erase(result.begin());
    }
    while (!result.empty() && result.back() == '-') {
        result.pop_back();
    }
    return result;
}

// -- Parser context ------------------------------------------------------------

struct ParseContext {
    std::unordered_map<std::string, std::string> node_defaults;
    std::unordered_map<std::string, std::string> edge_defaults;
    std::string subgraph_class;
    Graph graph;
};

// -- Token stream helper -------------------------------------------------------

struct TokenStream {
    const std::vector<Token>& tokens;
    std::size_t pos{0};

    const Token& peek(std::size_t offset = 0) const
    {
        std::size_t idx = pos + offset;
        if (idx < tokens.size()) {
            return tokens[idx];
        }
        static const Token eof_tok{TokenKind::eof, "", 0, 0};
        return eof_tok;
    }

    Token consume()
    {
        if (pos < tokens.size()) {
            return tokens[pos++];
        }
        static const Token eof_tok{TokenKind::eof, "", 0, 0};
        return eof_tok;
    }

    bool at_eof() const { return peek().kind == TokenKind::eof; }
};

// -- Attribute application -----------------------------------------------------

static auto apply_attrs_to_node(Node& node, const std::unordered_map<std::string, std::string>& attrs)
    -> std::optional<ParseError>
{
    for (const auto& [key, val] : attrs) {
        if (key == "label") {
            node.label = NodeLabel{val};
        }
        else if (key == "shape") {
            auto s = node_shape_from_string(val);
            if (!s) {
                return ParseError{"unknown shape: '" + val + "'", 0, 0};
            }
            node.shape = *s;
        }
        else if (key == "type") {
            node.node_type = HandlerTypeName{val};
        }
        else if (key == "prompt") {
            node.prompt = PromptText{val};
        }
        else if (key == "max_retries") {
            auto n_opt = parse_int_attr(val);
            if (!n_opt || *n_opt < 0) {
                return ParseError{"invalid value for attribute 'max_retries': '" + val + "'", 0, 0};
            }
            node.max_retries = MaxRetries{*n_opt};
        }
        else if (key == "goal_gate") {
            node.goal_gate = (val == "true");
        }
        else if (key == "retry_target") {
            node.retry_target = NodeId{val};
        }
        else if (key == "fallback_retry_target") {
            node.fallback_retry_target = NodeId{val};
        }
        else if (key == "fidelity") {
            auto f = parse_fidelity_mode(val);
            if (!f) {
                return ParseError{"invalid value for attribute 'fidelity': '" + val + "'", 0, 0};
            }
            node.fidelity = f;
        }
        else if (key == "thread_id") {
            node.thread_id = ThreadId{val};
        }
        else if (key == "class") {
            if (node.css_class.empty()) {
                node.css_class = CssClass{val};
            }
            else {
                type_safe::get(node.css_class) += ' ' + val;
            }
        }
        else if (key == "timeout") {
            auto d = parse_duration(val);
            if (!d) {
                return ParseError{"invalid value for attribute 'timeout': '" + val + "'", 0, 0};
            }
            node.timeout = d;
        }
        else if (key == "llm_model") {
            node.llm_model = LlmModel{val};
        }
        else if (key == "llm_provider") {
            node.llm_provider = LlmProvider{val};
        }
        else if (key == "reasoning_effort") {
            auto r = parse_reasoning_effort(val);
            if (!r) {
                return ParseError{"invalid value for attribute 'reasoning_effort': '" + val + "'", 0, 0};
            }
            node.reasoning_effort = r;
        }
        else if (key == "auto_status") {
            node.auto_status = (val == "true");
        }
        else if (key == "allow_partial") {
            node.allow_partial = (val == "true");
        }
        else if (key == "human.default_choice") {
            node.human_default_choice = NodeId{val};
        }
        else if (key == "tool_command") {
            node.tool_command = ShellCommand{val};
        }
        else if (key == "manager.stop_condition") {
            node.manager_stop_condition = ConditionExpr{val};
        }
        else if (key == "manager.max_cycles") {
            auto n_opt = parse_int_attr(val);
            if (!n_opt || *n_opt <= 0) {
                return ParseError{"invalid value for attribute 'manager.max_cycles': '" + val + "'", 0, 0};
            }
            node.manager_max_cycles = *n_opt;
        }
        else if (key == "join_policy") {
            if (val == "wait_all")
                node.join_policy = JoinPolicy::wait_all;
            else if (val == "first_success")
                node.join_policy = JoinPolicy::first_success;
            else
                return ParseError{"unknown join_policy: " + val};
        }
        else if (key == "max_parallel") {
            auto n_opt = parse_int_attr(val);
            if (!n_opt || *n_opt <= 0) {
                return ParseError{"invalid value for attribute 'max_parallel': '" + val + "'", 0, 0};
            }
            node.max_parallel = MaxParallel{*n_opt};
        }
    }
    return std::nullopt;
}

static auto apply_attrs_to_edge(Edge& edge, const std::unordered_map<std::string, std::string>& attrs)
    -> std::optional<ParseError>
{
    for (const auto& [key, val] : attrs) {
        if (key == "label") {
            edge.label = EdgeLabel{val};
        }
        else if (key == "condition") {
            edge.condition = ConditionExpr{val};
        }
        else if (key == "weight") {
            auto n_opt = parse_int_attr(val);
            if (!n_opt || *n_opt < 0) {
                return ParseError{"invalid value for attribute 'weight': '" + val + "'", 0, 0};
            }
            edge.weight = Weight{*n_opt};
        }
        else if (key == "fidelity") {
            auto f = parse_fidelity_mode(val);
            if (!f) {
                return ParseError{"invalid value for attribute 'fidelity': '" + val + "'", 0, 0};
            }
            edge.fidelity = f;
        }
        else if (key == "thread_id") {
            edge.thread_id = ThreadId{val};
        }
        else if (key == "loop_restart") {
            edge.loop_restart = (val == "true");
        }
    }
    return std::nullopt;
}

static auto apply_attrs_to_graph(Graph& graph, const std::unordered_map<std::string, std::string>& attrs)
    -> std::optional<ParseError>
{
    for (const auto& [key, val] : attrs) {
        if (key == "goal") {
            graph.goal = GoalText{val};
        }
        else if (key == "label") {
            graph.label = GraphLabel{val};
        }
        else if (key == "model_stylesheet") {
            graph.model_stylesheet = StylesheetId{val};
        }
        else if (key == "default_max_retries" || key == "default_max_retry") {
            auto n_opt = parse_int_attr(val);
            if (!n_opt || *n_opt < 0) {
                return ParseError{"invalid value for attribute '" + key + "': '" + val + "'", 0, 0};
            }
            graph.default_max_retries = MaxRetries{*n_opt};
        }
        else if (key == "default_fidelity") {
            auto f = parse_fidelity_mode(val);
            if (!f) {
                return ParseError{"invalid value for attribute 'default_fidelity': '" + val + "'", 0, 0};
            }
            graph.default_fidelity = f;
        }
        else if (key == "retry_target") {
            graph.retry_target = NodeId{val};
        }
        else if (key == "fallback_retry_target") {
            graph.fallback_retry_target = NodeId{val};
        }
        else if (key == "stack.child_dotfile") {
            graph.stack_child_dotfile = DotfilePath{val};
        }
        else if (key == "stack.child_workdir") {
            graph.stack_child_workdir = WorkDir{val};
        }
        else if (key == "tool_hooks.pre") {
            graph.tool_hooks_pre = ShellCommand{val};
        }
        else if (key == "tool_hooks.post") {
            graph.tool_hooks_post = ShellCommand{val};
        }
    }
    return std::nullopt;
}

// -- Forward declarations ------------------------------------------------------

using AttrMap = std::unordered_map<std::string, std::string>;

static auto parse_attr_block(TokenStream& ts) -> std::expected<AttrMap, ParseError>;
static auto parse_statement_list(TokenStream& ts, ParseContext& ctx, bool is_subgraph,
                                 std::string* subgraph_label_out = nullptr) -> std::optional<ParseError>;

// -- parse_attr_block ----------------------------------------------------------

static auto parse_value_as_string(TokenStream& ts) -> std::optional<std::string>
{
    const Token& t = ts.peek();
    if (t.kind == TokenKind::string_lit || t.kind == TokenKind::duration_lit || t.kind == TokenKind::bool_lit ||
        t.kind == TokenKind::integer_lit || t.kind == TokenKind::float_lit || t.kind == TokenKind::identifier ||
        t.kind == TokenKind::bare_value) {
        return ts.consume().value;
    }
    return std::nullopt;
}

static auto parse_attr_key(TokenStream& ts) -> std::optional<std::string>
{
    const Token& t = ts.peek();
    if (t.kind == TokenKind::identifier || t.kind == TokenKind::bare_value) {
        return ts.consume().value;
    }
    return std::nullopt;
}

static auto parse_attr_block(TokenStream& ts) -> std::expected<AttrMap, ParseError>
{
    AttrMap attrs;
    int open_line = ts.peek().line;
    int open_col = ts.peek().column;
    ts.consume();  // '['
    while (!ts.at_eof() && ts.peek().kind != TokenKind::rbracket) {
        auto key_opt = parse_attr_key(ts);
        if (!key_opt) {
            const Token& bad = ts.peek();
            return std::unexpected(ParseError{"expected attribute key", bad.line, bad.column});
        }
        if (ts.peek().kind != TokenKind::equals) {
            const Token& bad = ts.peek();
            return std::unexpected(ParseError{"expected '=' after attribute key", bad.line, bad.column});
        }
        ts.consume();  // '='
        auto val_opt = parse_value_as_string(ts);
        if (!val_opt) {
            const Token& bad = ts.peek();
            return std::unexpected(ParseError{"expected attribute value", bad.line, bad.column});
        }
        attrs[*key_opt] = *val_opt;
        if (ts.peek().kind == TokenKind::comma) {
            ts.consume();
        }
    }
    if (ts.at_eof()) {
        return std::unexpected(ParseError{"unterminated attribute block", open_line, open_col});
    }
    ts.consume();  // ']'
    return attrs;
}

// -- ensure_node_exists --------------------------------------------------------

static auto ensure_node_exists(ParseContext& ctx, const std::string& id) -> std::optional<ParseError>
{
    for (const auto& nd : ctx.graph.nodes) {
        if (type_safe::get(nd.id) == id) {
            return std::nullopt;
        }
    }
    Node nd;
    nd.id = NodeId{id};
    nd.label = NodeLabel{id};
    if (auto err = apply_attrs_to_node(nd, ctx.node_defaults)) {
        return err;
    }
    if (!ctx.subgraph_class.empty()) {
        if (nd.css_class.empty()) {
            nd.css_class = CssClass{ctx.subgraph_class};
        }
        else {
            type_safe::get(nd.css_class) += ' ' + ctx.subgraph_class;
        }
    }
    ctx.graph.nodes.push_back(std::move(nd));
    return std::nullopt;
}

// -- Statement parsers ---------------------------------------------------------

static auto parse_node_stmt(TokenStream& ts, ParseContext& ctx, const Token& id_tok) -> std::optional<ParseError>
{
    if (auto err = validate_node_id(id_tok.value, id_tok.line, id_tok.column)) {
        return err;
    }

    AttrMap attrs;
    if (ts.peek().kind == TokenKind::lbracket) {
        auto res = parse_attr_block(ts);
        if (!res) {
            return res.error();
        }
        attrs = std::move(*res);
    }
    if (ts.peek().kind == TokenKind::semicolon) {
        ts.consume();
    }

    // Find existing node or create new one
    Node* existing = nullptr;
    for (auto& nd : ctx.graph.nodes) {
        if (type_safe::get(nd.id) == id_tok.value) {
            existing = &nd;
            break;
        }
    }

    if (existing) {
        if (auto err = apply_attrs_to_node(*existing, attrs)) {
            return err;
        }
    }
    else {
        Node nd;
        nd.id = NodeId{id_tok.value};
        nd.label = NodeLabel{id_tok.value};
        // Apply context defaults first
        if (auto err = apply_attrs_to_node(nd, ctx.node_defaults)) {
            return err;
        }
        // Then apply explicit attrs (override)
        if (auto err = apply_attrs_to_node(nd, attrs)) {
            return err;
        }
        // Apply subgraph CSS class
        if (!ctx.subgraph_class.empty()) {
            if (nd.css_class.empty()) {
                nd.css_class = CssClass{ctx.subgraph_class};
            }
            else {
                type_safe::get(nd.css_class) += ' ' + ctx.subgraph_class;
            }
        }
        ctx.graph.nodes.push_back(std::move(nd));
    }
    return std::nullopt;
}

static auto parse_edge_stmt(TokenStream& ts, ParseContext& ctx, const Token& first_id_tok) -> std::optional<ParseError>
{
    std::vector<std::string> node_ids;
    node_ids.push_back(first_id_tok.value);

    while (ts.peek().kind == TokenKind::arrow) {
        ts.consume();  // '->'
        const Token& next_id = ts.peek();
        if (next_id.kind != TokenKind::identifier) {
            return ParseError{"expected node identifier after '->'", next_id.line, next_id.column};
        }
        ts.consume();
        if (auto err = validate_node_id(next_id.value, next_id.line, next_id.column)) {
            return err;
        }
        node_ids.push_back(next_id.value);
    }

    AttrMap attrs;
    if (ts.peek().kind == TokenKind::lbracket) {
        auto res = parse_attr_block(ts);
        if (!res) {
            return res.error();
        }
        attrs = std::move(*res);
    }
    if (ts.peek().kind == TokenKind::semicolon) {
        ts.consume();
    }

    for (std::size_t i = 0; i + 1 < node_ids.size(); ++i) {
        Edge edge;
        if (auto err = apply_attrs_to_edge(edge, ctx.edge_defaults)) {
            return err;
        }
        if (auto err = apply_attrs_to_edge(edge, attrs)) {
            return err;
        }
        edge.from = NodeId{node_ids[i]};
        edge.to = NodeId{node_ids[i + 1]};
        ctx.graph.edges.push_back(edge);
        if (auto err = ensure_node_exists(ctx, node_ids[i])) {
            return err;
        }
        if (auto err = ensure_node_exists(ctx, node_ids[i + 1])) {
            return err;
        }
    }
    return std::nullopt;
}

static auto parse_graph_attr_stmt(TokenStream& ts, ParseContext& ctx) -> std::optional<ParseError>
{
    ts.consume();  // 'graph'
    if (ts.peek().kind != TokenKind::lbracket) {
        const Token& bad = ts.peek();
        return ParseError{"expected '[' after 'graph'", bad.line, bad.column};
    }
    auto res = parse_attr_block(ts);
    if (!res) {
        return res.error();
    }
    if (auto err = apply_attrs_to_graph(ctx.graph, *res)) {
        return err;
    }
    if (ts.peek().kind == TokenKind::semicolon) {
        ts.consume();
    }
    return std::nullopt;
}

static auto parse_node_defaults_stmt(TokenStream& ts, ParseContext& ctx) -> std::optional<ParseError>
{
    ts.consume();  // 'node'
    if (ts.peek().kind != TokenKind::lbracket) {
        const Token& bad = ts.peek();
        return ParseError{"expected '[' after 'node'", bad.line, bad.column};
    }
    auto res = parse_attr_block(ts);
    if (!res) {
        return res.error();
    }
    for (auto& [k, v] : *res) {
        ctx.node_defaults[k] = v;
    }
    if (ts.peek().kind == TokenKind::semicolon) {
        ts.consume();
    }
    return std::nullopt;
}

static auto parse_edge_defaults_stmt(TokenStream& ts, ParseContext& ctx) -> std::optional<ParseError>
{
    ts.consume();  // 'edge'
    if (ts.peek().kind != TokenKind::lbracket) {
        const Token& bad = ts.peek();
        return ParseError{"expected '[' after 'edge'", bad.line, bad.column};
    }
    auto res = parse_attr_block(ts);
    if (!res) {
        return res.error();
    }
    for (auto& [k, v] : *res) {
        ctx.edge_defaults[k] = v;
    }
    if (ts.peek().kind == TokenKind::semicolon) {
        ts.consume();
    }
    return std::nullopt;
}

static auto parse_subgraph_stmt(TokenStream& ts, ParseContext& ctx) -> std::optional<ParseError>
{
    ts.consume();  // 'subgraph'

    std::string subgraph_label;

    // Optional subgraph id
    if (ts.peek().kind == TokenKind::identifier || ts.peek().kind == TokenKind::bare_value) {
        ts.consume();
    }

    if (ts.peek().kind != TokenKind::lbrace) {
        const Token& bad = ts.peek();
        return ParseError{"expected '{' for subgraph body", bad.line, bad.column};
    }
    ts.consume();  // '{'

    // Save defaults scope
    auto saved_node_defaults = ctx.node_defaults;
    auto saved_subgraph_class = ctx.subgraph_class;
    std::size_t nodes_before = ctx.graph.nodes.size();

    if (auto err = parse_statement_list(ts, ctx, true, &subgraph_label)) {
        return err;
    }

    if (ts.peek().kind != TokenKind::rbrace) {
        const Token& bad = ts.peek();
        return ParseError{"expected '}' to close subgraph", bad.line, bad.column};
    }
    ts.consume();  // '}'

    // Apply derived CSS class to all nodes added in this subgraph
    std::string derived_class = derive_css_class(subgraph_label);
    if (!derived_class.empty()) {
        for (std::size_t i = nodes_before; i < ctx.graph.nodes.size(); ++i) {
            Node& nd = ctx.graph.nodes[i];
            if (nd.css_class.empty()) {
                nd.css_class = CssClass{derived_class};
            }
            else {
                type_safe::get(nd.css_class) += ' ' + derived_class;
            }
        }
    }

    // Restore defaults scope
    ctx.node_defaults = saved_node_defaults;
    ctx.subgraph_class = saved_subgraph_class;

    if (ts.peek().kind == TokenKind::semicolon) {
        ts.consume();
    }
    return std::nullopt;
}

static auto parse_statement_list(TokenStream& ts, ParseContext& ctx, bool is_subgraph, std::string* subgraph_label_out)
    -> std::optional<ParseError>
{
    while (!ts.at_eof()) {
        const Token& t = ts.peek();

        if (t.kind == TokenKind::rbrace) {
            break;
        }

        if (t.kind == TokenKind::semicolon) {
            ts.consume();
            continue;
        }

        if (t.kind == TokenKind::identifier && t.value == "graph") {
            if (auto err = parse_graph_attr_stmt(ts, ctx)) {
                return err;
            }
            continue;
        }

        if (t.kind == TokenKind::identifier && t.value == "node") {
            if (auto err = parse_node_defaults_stmt(ts, ctx)) {
                return err;
            }
            continue;
        }

        if (t.kind == TokenKind::identifier && t.value == "edge") {
            if (auto err = parse_edge_defaults_stmt(ts, ctx)) {
                return err;
            }
            continue;
        }

        if (t.kind == TokenKind::identifier && t.value == "subgraph") {
            if (auto err = parse_subgraph_stmt(ts, ctx)) {
                return err;
            }
            continue;
        }

        if (t.kind == TokenKind::identifier) {
            // Disambiguate: edge stmt, node stmt, or bare key=value graph attr decl
            const Token& la = ts.peek(1);

            if (la.kind == TokenKind::equals) {
                Token key_tok = ts.consume();
                ts.consume();  // '='
                auto val_opt = parse_value_as_string(ts);
                if (!val_opt) {
                    const Token& bad = ts.peek();
                    return ParseError{"expected value after '='", bad.line, bad.column};
                }
                if (ts.peek().kind == TokenKind::semicolon) {
                    ts.consume();
                }
                // Capture subgraph label in subgraph context
                if (is_subgraph && subgraph_label_out != nullptr && key_tok.value == "label") {
                    *subgraph_label_out = *val_opt;
                    continue;
                }
                if (!is_subgraph) {
                    AttrMap single_attr;
                    single_attr[key_tok.value] = *val_opt;
                    if (auto err = apply_attrs_to_graph(ctx.graph, single_attr)) {
                        return err;
                    }
                }
                continue;
            }

            if (la.kind == TokenKind::arrow) {
                Token id_tok = ts.consume();
                if (auto err = validate_node_id(id_tok.value, id_tok.line, id_tok.column)) {
                    return err;
                }
                if (auto err = parse_edge_stmt(ts, ctx, id_tok)) {
                    return err;
                }
                continue;
            }

            // Node statement
            Token id_tok = ts.consume();
            if (auto err = parse_node_stmt(ts, ctx, id_tok)) {
                return err;
            }
            continue;
        }

        // bare_value, integer_lit, or float_lit in statement position -> invalid node ID
        if (t.kind == TokenKind::bare_value || t.kind == TokenKind::integer_lit || t.kind == TokenKind::float_lit) {
            return ParseError{"invalid node ID: " + t.value, t.line, t.column};
        }

        // Truly unexpected token
        return ParseError{"unexpected token: '" + t.value + "'", t.line, t.column};
    }
    return std::nullopt;
}

// -- Public API: parse_graph ---------------------------------------------------

auto parse_graph(std::string_view source) -> std::expected<Graph, ParseError>
{
    auto stripped_result = strip_comments(source);
    if (!stripped_result) {
        return std::unexpected(stripped_result.error());
    }
    std::string& stripped = *stripped_result;

    Lexer lexer{stripped};
    auto tokens = lexer.tokenize();

    // Check for '--' undirected edges and HTML labels
    for (const auto& tok : tokens) {
        if (tok.kind == TokenKind::dash_dash) {
            return std::unexpected(ParseError{"undirected '--' edges are not supported", tok.line, tok.column});
        }
        if (tok.kind == TokenKind::lt_angle) {
            return std::unexpected(ParseError{"HTML labels are not supported", tok.line, tok.column});
        }
    }

    TokenStream ts{tokens};

    // Check for 'strict'
    if (ts.peek().kind == TokenKind::identifier && ts.peek().value == "strict") {
        const Token& t = ts.peek();
        return std::unexpected(ParseError{"'strict' graphs are not supported", t.line, t.column});
    }

    // Expect 'digraph'
    if (ts.peek().kind != TokenKind::identifier || ts.peek().value != "digraph") {
        const Token& t = ts.peek();
        return std::unexpected(ParseError{"expected 'digraph'", t.line, t.column});
    }
    ts.consume();

    ParseContext ctx;

    // Optional digraph id
    if (ts.peek().kind == TokenKind::identifier) {
        ctx.graph.digraph_id = GraphId{ts.consume().value};
    }

    // '{'
    if (ts.peek().kind != TokenKind::lbrace) {
        const Token& t = ts.peek();
        return std::unexpected(ParseError{"expected '{' after digraph id", t.line, t.column});
    }
    ts.consume();

    if (auto err = parse_statement_list(ts, ctx, false)) {
        return std::unexpected(*err);
    }

    // '}'
    if (ts.peek().kind != TokenKind::rbrace) {
        const Token& t = ts.peek();
        return std::unexpected(ParseError{"expected '}'", t.line, t.column});
    }
    ts.consume();

    // Apply default labels: label = id string where label is empty
    for (auto& nd : ctx.graph.nodes) {
        if (nd.label.empty()) {
            nd.label = NodeLabel{type_safe::get(nd.id)};
        }
    }

    return ctx.graph;
}

}  // namespace attractor
