#include <attractor/interviewer.hpp>

namespace attractor {

RecordingInterviewer::RecordingInterviewer(Interviewer& inner)
    : m_inner(&inner)
{
}

auto RecordingInterviewer::ask(const Question& question) -> Answer
{
    Answer answer = m_inner->ask(question);
    m_recordings.push_back(Recording{.question = question, .answer = answer});
    return answer;
}

void RecordingInterviewer::inform(const std::string& message, const std::string& stage)
{
    m_inner->inform(message, stage);
}

auto RecordingInterviewer::recordings() const -> const std::vector<Recording>&
{
    return m_recordings;
}

}  // namespace attractor
