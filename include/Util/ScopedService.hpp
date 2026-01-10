#ifndef __SCOPED_SERVICE_HPP__
#define __SCOPED_SERVICE_HPP__

#include <3ds.h>
#include <functional>

class ScopedService {
public:
    // only calls provided destruct function if valid
    ScopedService(const std::function<Result()>& onInit, const std::function<Result()>& onDestruct);
    virtual ~ScopedService();

    bool valid() const;
    Result result() const;

protected:
    bool m_valid;
    Result m_result;

    std::function<void()> m_destructFunc;
};

namespace Services {
class AM : public ScopedService {
public:
    AM();
};

class RomFS : public ScopedService {
public:
    RomFS();
};

class PTMU : public ScopedService {
public:
    PTMU();
};

class MCUHWc : public ScopedService {
public:
    MCUHWc();
};
}; // namespace Services

#endif