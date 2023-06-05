#ifndef NON_COPYABLE_H
#define NON_COPYABLE_H

// noncopyable for locker, etc.

class noncopyable {
public:
    noncopyable(const noncopyable&) = delete;
    void operator=(const noncopyable&) = delete;

protected:
    noncopyable() = default;
    ~noncopyable() = default;
};


#endif // NON_COPYABLE_H
