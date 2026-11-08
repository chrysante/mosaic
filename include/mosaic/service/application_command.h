#pragma once

#include <functional>

namespace mosaic {

class Application;

class ApplicationCommand {
public:
    static ApplicationCommand
    create(std::function<void(Application&)> function) {
        return ApplicationCommand(std::move(function));
    }

    void execute(Application& application) const { m_function(application); }

private:
    explicit ApplicationCommand(std::function<void(Application&)>&& function)
        : m_function(std::move(function)) {}

    std::function<void(Application&)> m_function;
};

} // namespace mosaic
