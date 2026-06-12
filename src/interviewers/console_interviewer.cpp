#include <attractor/interviewer.hpp>

#include <cctype>
#include <iostream>
#include <string>

namespace attractor {

ConsoleInterviewer::ConsoleInterviewer()
    : m_in(&std::cin), m_out(&std::cout)
{
}

ConsoleInterviewer::ConsoleInterviewer(std::istream& in, std::ostream& out)
    : m_in(&in), m_out(&out)
{
}

auto ConsoleInterviewer::ask(const Question& question) -> Answer
{
    *m_out << "[?] " << question.text << "\n";

    if (question.type == QuestionType::multiple_choice) {
        for (const auto& opt : question.options)
            *m_out << "  " << opt.label << "\n";
        *m_out << "Select: ";
        std::string input;
        std::getline(*m_in, input);
        const std::string key =
            input.empty() ? "" : std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(input[0]))));
        for (const auto& opt : question.options) {
            if (key == opt.key)
                return Answer{.kind = AnswerKind::text, .text = opt.key, .selected_option = opt};
        }
        if (!question.options.empty())
            return Answer{.kind = AnswerKind::text,
                          .text = question.options[0].key,
                          .selected_option = question.options[0]};
        return Answer{.kind = AnswerKind::skipped};
    }

    if (question.type == QuestionType::yes_no || question.type == QuestionType::confirmation) {
        *m_out << "[Y/N]: ";
        std::string input;
        std::getline(*m_in, input);
        if (!input.empty() && (input[0] == 'Y' || input[0] == 'y'))
            return Answer{.kind = AnswerKind::yes};
        if (!input.empty() && (input[0] == 'N' || input[0] == 'n'))
            return Answer{.kind = AnswerKind::no};
        return Answer{.kind = AnswerKind::skipped};
    }

    *m_out << "> ";
    std::string input;
    if (!std::getline(*m_in, input))
        return Answer{.kind = AnswerKind::skipped};
    return Answer{.kind = AnswerKind::text, .text = input};
}

void ConsoleInterviewer::inform(const std::string& message, const std::string& stage)
{
    *m_out << "[" << stage << "] " << message << "\n";
}

}  // namespace attractor
