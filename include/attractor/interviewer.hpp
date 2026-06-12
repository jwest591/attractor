#ifndef ATTRACTOR_INTERVIEWER_HPP
#define ATTRACTOR_INTERVIEWER_HPP

#include <attractor/types.hpp>

#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace attractor {

// -- Data types

struct Option {
    std::string key;
    std::string label;
};

struct Question {
    std::string text;
    QuestionType type{ QuestionType::freeform };
    std::vector<Option> options{};               // NOLINT(readability-redundant-member-init)
    std::optional<std::string> default_answer{}; // NOLINT(readability-redundant-member-init)
    std::optional<double> timeout_seconds{};     // NOLINT(readability-redundant-member-init)
    std::string stage{};                         // NOLINT(readability-redundant-member-init)
};

struct Answer {
    AnswerKind kind{ AnswerKind::skipped };
    std::string text{};                          // NOLINT(readability-redundant-member-init)
    std::optional<Option> selected_option{};     // NOLINT(readability-redundant-member-init)
};

struct Recording {
    Question question;
    Answer answer;
};

// -- Interviewer base

class Interviewer {
public:
    virtual ~Interviewer() = default;

    [[nodiscard]] virtual auto ask(const Question& question) -> Answer = 0;

    [[nodiscard]] virtual auto ask_multiple(const std::vector<Question>& questions)
        -> std::vector<Answer>;

    virtual void inform(const std::string& message, const std::string& stage);
};

// -- AutoApproveInterviewer

class AutoApproveInterviewer final : public Interviewer {
public:
    [[nodiscard]] auto ask(const Question& question) -> Answer override;
    void inform(const std::string& message, const std::string& stage) override;
};

// -- QueueInterviewer

class QueueInterviewer final : public Interviewer {
public:
    void push(Answer answer);
    [[nodiscard]] auto ask(const Question& question) -> Answer override;
    void inform(const std::string& message, const std::string& stage) override;

private:
    std::queue<Answer> m_answers;
};

// -- RecordingInterviewer

class RecordingInterviewer final : public Interviewer {
public:
    explicit RecordingInterviewer(Interviewer& inner);

    [[nodiscard]] auto ask(const Question& question) -> Answer override;
    void inform(const std::string& message, const std::string& stage) override;

    [[nodiscard]] auto recordings() const -> const std::vector<Recording>&;

private:
    Interviewer* m_inner;
    std::vector<Recording> m_recordings;
};

}  // namespace attractor

#endif  // ATTRACTOR_INTERVIEWER_HPP
