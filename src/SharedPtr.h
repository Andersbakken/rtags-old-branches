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
    // SharedPtr<T> lock() const { return SharedPtr<T>(std::weak_ptr<T>::lock()); }
};

#endif
