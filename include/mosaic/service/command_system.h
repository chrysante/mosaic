#pragma once

#include <vector>

#include <mosaic/service/application_command.h>

namespace mosaic {

namespace internal { struct ApplicationImpl; }

class CommandSystem {
public:
    void submit(ApplicationCommand command);

private:
    friend struct internal::ApplicationImpl;

    void process(Application& application);

    std::vector<ApplicationCommand> m_buffer, m_back_buffer;
};

} // namespace mosaic