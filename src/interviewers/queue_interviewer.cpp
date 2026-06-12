#include <attractor/interviewer.hpp>

namespace attractor {

void QueueInterviewer::push(Answer answer)
{
    m_answers.push(std::move(answer));
}

auto QueueInterviewer::ask(const Question& /*question*/) -> Answer
{
    if (m_answers.empty())
        return Answer{.kind = AnswerKind::skipped};

    Answer front = std::move(m_answers.front());
    m_answers.pop();
    return front;
}

void QueueInterviewer::inform(const std::string& /*message*/, const std::string& /*stage*/)
{
}

}  // namespace attractor
