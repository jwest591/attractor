#include <attractor/interviewer.hpp>

namespace attractor {

auto Interviewer::ask_multiple(const std::vector<Question>& questions) -> std::vector<Answer>
{
    std::vector<Answer> answers;
    answers.reserve(questions.size());
    for (const auto& q : questions)
        answers.push_back(ask(q));
    return answers;
}

void Interviewer::inform(const std::string& /*message*/, const std::string& /*stage*/)
{
}

// -- AutoApproveInterviewer

auto AutoApproveInterviewer::ask(const Question& question) -> Answer
{
    if (question.type == QuestionType::yes_no || question.type == QuestionType::confirmation)
        return Answer{.kind = AnswerKind::yes};

    if (question.type == QuestionType::multiple_choice && !question.options.empty())
        return Answer{
            .kind = AnswerKind::text,
            .text = question.options[0].key,
            .selected_option = question.options[0],
        };

    return Answer{.kind = AnswerKind::text, .text = "auto-approved"};
}

void AutoApproveInterviewer::inform(const std::string& /*message*/, const std::string& /*stage*/)
{
}

}  // namespace attractor
