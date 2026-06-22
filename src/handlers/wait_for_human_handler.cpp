#include <attractor/handlers/wait_for_human_handler.hpp>

#include <attractor/context.hpp>
#include <attractor/graph.hpp>
#include <attractor/handler.hpp>
#include <attractor/interviewer.hpp>
#include <attractor/types.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace attractor {

namespace {

auto parse_accelerator_key(const std::string& label) -> std::string
{
    if (label.size() >= 3 && label[0] == '[' && label[2] == ']'
            && std::isprint(static_cast<unsigned char>(label[1])))
        return std::string(1, label[1]);
    if (label.size() >= 2 && label[1] == ')'
            && std::isprint(static_cast<unsigned char>(label[0])))
        return std::string(1, label[0]);
    if (label.size() >= 3 && label[1] == ' ' && label[2] == '-'
            && std::isprint(static_cast<unsigned char>(label[0])))
        return std::string(1, label[0]);
    if (!label.empty() && std::isprint(static_cast<unsigned char>(label[0])))
        return std::string(1, label[0]);
    return "";
}

}  // namespace

WaitForHumanHandler::WaitForHumanHandler(Interviewer& interviewer)
    : m_interviewer{&interviewer}
{}

auto WaitForHumanHandler::execute(const Node& node, Context& /*ctx*/, const Graph& graph,
                                   const LogsRoot& /*logs_root*/) const -> Outcome
{
    std::vector<const Edge*> outgoing;
    for (const auto& e : graph.edges) {
        if (e.from == node.id) {
            outgoing.push_back(&e);
        }
    }

    if (outgoing.empty()) {
        return Outcome::fail(
            DiagnosticMessage{"No outgoing edges for human gate node: " + type_safe::get(node.id)});
    }

    std::vector<Option> options;
    options.reserve(outgoing.size());
    for (const Edge* e : outgoing) {
        const std::string label_str = type_safe::get(e->label);
        options.push_back(Option{.key = parse_accelerator_key(label_str), .label = label_str});
    }

    const std::string node_label_str = type_safe::get(node.label);
    Question question;
    question.text    = node_label_str.empty() ? "Select an option:" : node_label_str;
    question.type    = QuestionType::multiple_choice;
    question.options = options;
    question.stage   = type_safe::get(node.id);

    Answer answer = m_interviewer->ask(question);

    if (answer.kind == AnswerKind::timeout) {
        const std::string default_id = type_safe::get(node.human_default_choice);
        if (!default_id.empty()) {
            for (const Edge* e : outgoing) {
                if (type_safe::get(e->to) == default_id) {
                    return Outcome{.status         = StageStatus::success,
                                   .preferred_label = e->label};
                }
            }
        }
        return Outcome{.status          = StageStatus::retry,
                       .failure_reason  = DiagnosticMessage{"human gate timeout, no default"}};
    }

    if (answer.kind == AnswerKind::skipped) {
        return Outcome::fail(DiagnosticMessage{"human skipped interaction"});
    }

    if (answer.kind == AnswerKind::yes) { answer.text = "Y"; }
    else if (answer.kind == AnswerKind::no) { answer.text = "N"; }

    std::string answer_upper = answer.text;
    std::transform(answer_upper.begin(), answer_upper.end(), answer_upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    const Edge* matched = nullptr;
    for (std::size_t i = 0; i < outgoing.size(); ++i) {
        std::string key_upper = options[i].key;
        std::transform(key_upper.begin(), key_upper.end(), key_upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (!key_upper.empty() && key_upper == answer_upper) {
            matched = outgoing[i];
            break;
        }
    }

    if (matched == nullptr) {
        matched = outgoing[0];
    }

    const std::string matched_label = type_safe::get(matched->label);
    const std::string matched_key   = parse_accelerator_key(matched_label);

    auto updates = nlohmann::json::object();
    updates["human.gate.selected"] = matched_key;
    updates["human.gate.label"]    = matched_label;

    return Outcome{.status          = StageStatus::success,
                   .preferred_label = matched->label,
                   .context_updates = std::move(updates)};
}

}  // namespace attractor
