#ifndef SharedPtr_h
#define SharedPtr_h

#include <memory>

template <typename T>
class SharedPtr : public std::shared_ptr<T>
{
public:
    SharedPtr(T *t = 0)
        : std::shared_ptr<T>(t)
    {}
    SharedPtr(const std::shared_ptr<T> &other)
        : std::shared_ptr<T>(other)
    {}
};

template <typename T>
class WeakPtr : public std::weak_ptr<T>
{
public:
    WeakPtr(const SharedPtr<T> &t)
        : std::weak_ptr<T>(t)
    {}
    WeakPtr()
    {}
    // SharedPtr<T> lock() const { return SharedPtr<T>(std::weak_ptr<T>::lock()); }
};

template <typename T>
class EnableSharedFromThis : public std::enable_shared_from_this<T>
{
public:
};

#endif
