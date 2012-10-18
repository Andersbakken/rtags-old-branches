#ifndef SharedPtr_h
#define SharedPtr_h

#include <memory>

template <typename T>
class SharedPtr : public std::shared_ptr<T>
{
public:
};

template <typename T>
class WeakPtr : public std::weak_ptr<T>
{
public:
};

#endif
