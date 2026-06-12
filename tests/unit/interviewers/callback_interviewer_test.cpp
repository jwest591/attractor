#include "attractor_test_support.hpp"

#include <attractor/interviewer.hpp>

using namespace attractor;

SNITCH_TEST_CASE("[callback_interviewer] invokes callback and returns answer -- 3.3-U-011")
{
    CallbackInterviewer ci{[](const Question& /*q*/) {
        return Answer{.kind = AnswerKind::yes};
    }};
    const Question q{.text = "Go?", .type = QuestionType::yes_no};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[callback_interviewer] forwards question to callback -- 3.3-U-012")
{
    std::string captured_text;
    CallbackInterviewer ci{[&captured_text](const Question& q) {
        captured_text = q.text;
        return Answer{.kind = AnswerKind::no};
    }};
    const Question q{.text = "Are you sure?", .type = QuestionType::yes_no};
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::no);
    SNITCH_CHECK(captured_text == "Are you sure?");
}

SNITCH_TEST_CASE("[callback_interviewer] returns exact answer from callback -- 3.3-U-013")
{
    const Option expected_opt{.key = "A", .label = "[A] Approve"};
    CallbackInterviewer ci{[expected_opt](const Question& /*q*/) {
        return Answer{.kind = AnswerKind::text, .text = "A", .selected_option = expected_opt};
    }};
    const Question q{
        .text = "Choose",
        .type = QuestionType::multiple_choice,
        .options = {expected_opt},
    };
    const Answer answer = ci.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "A");
    SNITCH_REQUIRE(answer.selected_option.has_value());
    SNITCH_CHECK(answer.selected_option->key == "A");
}

SNITCH_TEST_CASE("[callback_interviewer] callable through Interviewer interface -- 3.3-U-014")
{
    CallbackInterviewer ci{[](const Question& /*q*/) {
        return Answer{.kind = AnswerKind::yes};
    }};
    Interviewer& iface = ci;
    const Question q{.text = "Go?", .type = QuestionType::yes_no};
    const Answer answer = iface.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}
