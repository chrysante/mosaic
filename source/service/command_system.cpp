#include "service/command_system.h"

using namespace mosaic;

void CommandSystem::submit(ApplicationCommand command) {
    m_buffer.push_back(std::move(command));
}

void CommandSystem::process(Application& app) {
    std::swap(m_buffer, m_back_buffer);
    for (auto& command : m_back_buffer)
        command.execute(app);
    m_back_buffer.clear();
}