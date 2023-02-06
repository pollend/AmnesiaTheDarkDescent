#pragma once

class BootstrapEngine {
public:
    BootstrapEngine();
    ~BootstrapEngine();

    virtual void Run() = 0;
private:
};