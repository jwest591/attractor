#include "attractor_test_support.hpp"

#include <attractor/interviewer.hpp>

using namespace attractor;

SNITCH_TEST_CASE("[recording_interviewer] records Q&A pairs from wrapped interviewer -- 3.1-U-013")
{
    AutoApproveInterviewer inner;
    RecordingInterviewer ri{inner};

    const Question q1{.text = "Q1", .type = QuestionType::yes_no};
    const Question q2{.text = "Q2", .type = QuestionType::yes_no};
    (void)ri.ask(q1);
    (void)ri.ask(q2);

    const auto& recs = ri.recordings();
    SNITCH_REQUIRE(recs.size() == 2);
    SNITCH_CHECK(recs[0].question.text == "Q1");
    SNITCH_CHECK(recs[0].answer.kind == AnswerKind::yes);
    SNITCH_CHECK(recs[1].question.text == "Q2");
    SNITCH_CHECK(recs[1].answer.kind == AnswerKind::yes);
}

SNITCH_TEST_CASE("[recording_interviewer] returns inner interviewer answer unchanged -- 3.1-U-014")
{
    AutoApproveInterviewer inner;
    RecordingInterviewer ri{inner};

    const Question q{
        .text = "Choose",
        .type = QuestionType::multiple_choice,
        .options = {Option{.key = "X", .label = "[X] Go"}},
    };
    const Answer answer = ri.ask(q);

    SNITCH_CHECK(answer.kind == AnswerKind::text);
    SNITCH_CHECK(answer.text == "X");
    SNITCH_REQUIRE(answer.selected_option.has_value());
    SNITCH_CHECK(answer.selected_option->key == "X");
}

SNITCH_TEST_CASE("[recording_interviewer] empty recordings before any ask -- 3.1-U-015")
{
    AutoApproveInterviewer inner;
    RecordingInterviewer ri{inner};
    SNITCH_CHECK(ri.recordings().empty());
}

SNITCH_TEST_CASE("[recording_interviewer] wraps QueueInterviewer; records queue answers -- 3.1-U-016")
{
    QueueInterviewer inner;
    inner.push(Answer{.kind = AnswerKind::no});

    RecordingInterviewer ri{inner};
    const Question q{.text = "Go?", .type = QuestionType::yes_no};
    const Answer answer = ri.ask(q);

    SNITCH_CHECK(answer.kind == AnswerKind::no);
    SNITCH_REQUIRE(ri.recordings().size() == 1);
    SNITCH_CHECK(ri.recordings()[0].answer.kind == AnswerKind::no);
}

SNITCH_TEST_CASE("[recording_interviewer] callable through Interviewer interface -- 3.1-U-017")
{
    AutoApproveInterviewer inner;
    RecordingInterviewer ri{inner};
    Interviewer& iface = ri;

    const Question q{.text = "Go?", .type = QuestionType::yes_no};
    const Answer answer = iface.ask(q);
    SNITCH_CHECK(answer.kind == AnswerKind::yes);
    SNITCH_CHECK(ri.recordings().size() == 1);
}
