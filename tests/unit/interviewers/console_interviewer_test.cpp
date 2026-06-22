#include "attractor_test_support.hpp"

#include <attractor/interviewer.hpp>

#include <sstream>

using namespace attractor;

SNITCH_TEST_CASE("[console_interviewer] YES_NO 'Y' returns yes -- 3.3-U-001")
{
    std::istringstream in{"Y\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Proceed?", .type = QuestionType::yes_no};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[console_interviewer] YES_NO 'y' (lowercase) returns yes -- 3.3-U-002")
{
    std::istringstream in{"y\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Proceed?", .type = QuestionType::yes_no};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[console_interviewer] YES_NO 'N' returns no -- 3.3-U-003")
{
    std::istringstream in{"N\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Proceed?", .type = QuestionType::yes_no};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::no);
}

SNITCH_TEST_CASE("[console_interviewer] YES_NO EOF returns skipped -- 3.3-U-004")
{
    std::istringstream in{""};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Proceed?", .type = QuestionType::yes_no};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::skipped);
}

SNITCH_TEST_CASE("[console_interviewer] MULTIPLE_CHOICE matching key returns option -- 3.3-U-005")
{
    std::istringstream in{"F\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{
        .text = "Choose action",
        .type = QuestionType::multiple_choice,
        .options = {Option{.key = "A", .label = "[A] Approve"}, Option{.key = "F", .label = "[F] Fix"}},
    };
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "F");
    SNITCH_REQUIRE(answer.selected_option.has_value());
    SNITCH_CHECK(answer.selected_option->key == "F");
    SNITCH_CHECK(answer.selected_option->label == "[F] Fix");
}

SNITCH_TEST_CASE("[console_interviewer] MULTIPLE_CHOICE lowercase key matches -- 3.3-U-006")
{
    std::istringstream in{"a\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{
        .text = "Choose",
        .type = QuestionType::multiple_choice,
        .options = {Option{.key = "A", .label = "[A] Approve"}, Option{.key = "F", .label = "[F] Fix"}},
    };
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "A");
}

SNITCH_TEST_CASE("[console_interviewer] MULTIPLE_CHOICE unrecognised key falls back to first option -- 3.3-U-007")
{
    std::istringstream in{"Z\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{
        .text = "Choose",
        .type = QuestionType::multiple_choice,
        .options = {Option{.key = "A", .label = "[A] Approve"}, Option{.key = "F", .label = "[F] Fix"}},
    };
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "A");
    SNITCH_REQUIRE(answer.selected_option.has_value());
    SNITCH_CHECK(answer.selected_option->key == "A");
}

SNITCH_TEST_CASE("[console_interviewer] FREEFORM returns input as text -- 3.3-U-008")
{
    std::istringstream in{"hello world\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Describe it", .type = QuestionType::freeform};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "hello world");
}

SNITCH_TEST_CASE("[console_interviewer] CONFIRMATION 'Y' returns yes -- 3.3-U-009")
{
    std::istringstream in{"Y\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Are you sure?", .type = QuestionType::confirmation};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[console_interviewer] callable through Interviewer interface -- 3.3-U-010")
{
    std::istringstream in{"Y\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    Interviewer& iface = ci;
    const Question q{.text = "Go?", .type = QuestionType::yes_no};
    const Answer answer = iface.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[console_interviewer] FREEFORM EOF returns skipped -- 3.3-U-015")
{
    std::istringstream in{""};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Describe it", .type = QuestionType::freeform};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::skipped);
}

SNITCH_TEST_CASE("[console_interviewer] MULTIPLE_CHOICE empty options returns skipped -- 7.5-U-001")
{
    std::istringstream in{""};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Choose", .type = QuestionType::multiple_choice, .options = {}};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::skipped);
    SNITCH_CHECK(out.str().empty());
}

SNITCH_TEST_CASE("[console_interviewer] MULTIPLE_CHOICE default_answer used on empty input -- 7.5-U-002")
{
    std::istringstream in{"\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{
        .text = "Choose action",
        .type = QuestionType::multiple_choice,
        .options = {Option{.key = "A", .label = "[A] Approve"}, Option{.key = "F", .label = "[F] Fix"}},
        .default_answer = "F",
    };
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "F");
    SNITCH_REQUIRE(answer.selected_option.has_value());
    SNITCH_CHECK(answer.selected_option->key == "F");
}

SNITCH_TEST_CASE("[console_interviewer] YES_NO default_answer Y on empty input returns yes -- 7.5-U-003")
{
    std::istringstream in{"\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Proceed?", .type = QuestionType::yes_no, .default_answer = "Y"};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[console_interviewer] YES_NO default_answer N on empty input returns no -- 7.5-U-004")
{
    std::istringstream in{"\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Proceed?", .type = QuestionType::yes_no, .default_answer = "N"};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::no);
}

SNITCH_TEST_CASE("[console_interviewer] FREEFORM default_answer used on empty input -- 7.5-U-005")
{
    std::istringstream in{"\n"};
    std::ostringstream out;
    ConsoleInterviewer ci{in, out};
    const Question q{.text = "Describe it", .type = QuestionType::freeform, .default_answer = "hello"};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "hello");
}
