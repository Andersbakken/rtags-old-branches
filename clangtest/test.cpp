#include "h.h"

struct Member 
{
    Member(int = 12)
    {}

};
class A;
class A
{
public:
    Member member;
    A(int = 13);
    A(const A &a) : member(a.member) {}

    int test;

    operator int() const { return test; }
};

void foo(A a)
{
    ++a.test;
}

void impl(int i)
{

}

typedef struct {
    int balle;
} fisk;

int main()
{
    struct Bar {
        int a, b;
    } foo;
    // A a;
    // A aa = 12;
    // A aaa(12);
    // A aaaa = A();
    A aaaaa = A(12);
    // A();
    // A(12);
    // foo(a);
    // foo(12);
    // foo(A(12));
    int balle = 123;

    int bar = aaaaa;
    impl(aaaaa);
    ++balle;
    return Test;
}

A::A(int val)
    : test(val), member(val)
{

}
