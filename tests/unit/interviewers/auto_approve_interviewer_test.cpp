#include "attractor_test_support.hpp"

#include <attractor/interviewer.hpp>

using namespace attractor;

SNITCH_TEST_CASE("[auto_approve_interviewer] YES_NO question returns yes -- 3.1-U-001")
{
    AutoApproveInterviewer ai;
    const Question q{.text = "Proceed?", .type = QuestionType::yes_no};
    const Answer answer = ai.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[auto_approve_interviewer] CONFIRMATION question returns yes -- 3.1-U-002")
{
    AutoApproveInterviewer ai;
    const Question q{.text = "Are you sure?", .type = QuestionType::confirmation};
    const Answer answer = ai.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[auto_approve_interviewer] MULTIPLE_CHOICE returns first option -- 3.1-U-003")
{
    AutoApproveInterviewer ai;
    const Question q{
        .text = "Choose action",
        .type = QuestionType::multiple_choice,
        .options = {Option{.key = "A", .label = "[A] Approve"}, Option{.key = "F", .label = "[F] Fix"}},
    };
    const Answer answer = ai.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "A");
    SNITCH_REQUIRE(answer.selected_option.has_value());
    SNITCH_CHECK(answer.selected_option->key == "A");
    SNITCH_CHECK(answer.selected_option->label == "[A] Approve");
}

SNITCH_TEST_CASE("[auto_approve_interviewer] MULTIPLE_CHOICE with no options returns text -- 3.1-U-004")
{
    AutoApproveInterviewer ai;
    const Question q{.text = "Choose", .type = QuestionType::multiple_choice};
    const Answer answer = ai.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "auto-approved");
}

SNITCH_TEST_CASE("[auto_approve_interviewer] FREEFORM question returns auto-approved text -- 3.1-U-005")
{
    AutoApproveInterviewer ai;
    const Question q{.text = "Describe the issue", .type = QuestionType::freeform};
    const Answer answer = ai.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "auto-approved");
}

SNITCH_TEST_CASE("[auto_approve_interviewer] callable through Interviewer interface -- 3.1-U-006")
{
    AutoApproveInterviewer ai;
    Interviewer& iface = ai;
    const Question q{.text = "Go?", .type = QuestionType::yes_no};
    const Answer answer = iface.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[auto_approve_interviewer] ask_multiple returns one answer per question -- 3.1-U-007")
{
    AutoApproveInterviewer ai;
    const std::vector<Question> questions{
        Question{.text = "Q1", .type = QuestionType::yes_no},
        Question{.text = "Q2", .type = QuestionType::yes_no},
    };
    const auto answers = ai.ask_multiple(questions);
    SNITCH_REQUIRE(answers.size() == 2);
    SNITCH_CHECK(answers[0].kind == AnswerKind::yes);
    SNITCH_CHECK(answers[1].kind == AnswerKind::yes);
}
