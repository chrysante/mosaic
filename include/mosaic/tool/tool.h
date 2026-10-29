#pragma once

namespace mosaic {

class ToolManager;

class Tool {
public:
    virtual ~Tool() = default;

private:
    friend class ToolManager;

    virtual void update() = 0;
};

} // namespace mosaic