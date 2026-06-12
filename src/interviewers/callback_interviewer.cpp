#include <attractor/interviewer.hpp>

#include <cassert>

namespace attractor {

CallbackInterviewer::CallbackInterviewer(Callback callback)
    : m_callback(std::move(callback))
{
    assert(static_cast<bool>(m_callback));
}

auto CallbackInterviewer::ask(const Question& question) -> Answer
{
    return m_callback(question);
}

}  // namespace attractor
