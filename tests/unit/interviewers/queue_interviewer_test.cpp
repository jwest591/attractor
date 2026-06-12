#include "attractor_test_support.hpp"

#include <attractor/interviewer.hpp>

using namespace attractor;

SNITCH_TEST_CASE("[queue_interviewer] empty queue returns SKIPPED -- 3.1-U-008")
{
    QueueInterviewer qi;
    const Question q{.text = "Go?", .type = QuestionType::yes_no};
    const Answer answer = qi.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::skipped);
}

SNITCH_TEST_CASE("[queue_interviewer] dequeues answers in order -- 3.1-U-009")
{
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::yes});
    qi.push(Answer{.kind = AnswerKind::no});

    const Question q{.text = "Go?", .type = QuestionType::yes_no};

    const Answer first = qi.ask(q);
    SNITCH_CHECK(first.kind == AnswerKind::yes);

    const Answer second = qi.ask(q);
    SNITCH_CHECK(second.kind == AnswerKind::no);
}

SNITCH_TEST_CASE("[queue_interviewer] third call returns SKIPPED after two pre-filled answers -- 3.1-U-010")
{
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::yes});
    qi.push(Answer{.kind = AnswerKind::no});

    const Question q{.text = "Go?", .type = QuestionType::yes_no};
    (void)qi.ask(q);
    (void)qi.ask(q);

    const Answer third = qi.ask(q);
    SNITCH_CHECK(third.kind == AnswerKind::skipped);
}

SNITCH_TEST_CASE("[queue_interviewer] preserves text and selected_option -- 3.1-U-011")
{
    QueueInterviewer qi;
    const Option opt{.key = "A", .label = "[A] Approve"};
    qi.push(Answer{.kind = AnswerKind::text, .text = "A", .selected_option = opt});

    const Question q{.text = "Choose", .type = QuestionType::multiple_choice};
    const Answer answer = qi.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "A");
    SNITCH_REQUIRE(answer.selected_option.has_value());
    SNITCH_CHECK(answer.selected_option->key == "A");
}

SNITCH_TEST_CASE("[queue_interviewer] callable through Interviewer interface -- 3.1-U-012")
{
    QueueInterviewer qi;
    qi.push(Answer{.kind = AnswerKind::yes});

    Interviewer& iface = qi;
    const Question q{.text = "Go?", .type = QuestionType::yes_no};
    const Answer answer = iface.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
}
