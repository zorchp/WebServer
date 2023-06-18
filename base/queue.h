#ifndef QUEUE_H
#define QUEUE_H
#include <cstddef>
#include <memory> // shared_ptr

template <typename T> // http_conn*
class queue {
private:
    struct node { // based on linked list
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    std::unique_ptr<node> head;
    node* tail;
    size_t m_size;

    node* get_tail() { // for check empty
        return tail;
    }

    std::unique_ptr<node> pop_head() {
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        --m_size;
        return old_head;
    }


public:
    queue() : head(new node), tail(head.get()), m_size(0) {
    }
    queue(const queue& rhs) = delete;
    queue& operator=(const queue& rhs) = delete;

    std::shared_ptr<T> pop() {
        std::unique_ptr<node> old_head = pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
    void push(T new_val) {
        std::unique_ptr<node> p(new node);
        auto new_data(std::make_shared<T>(std::move(new_val)));
        tail->data = new_data;
        node* const new_tail = p.get();
        tail->next = std::move(p);
        tail = new_tail;
        ++m_size;
    }
    bool empty() { // not const
        return head.get() == get_tail();
    }
    size_t size() const {
        return m_size;
    }
};

#endif // !QUEUE_H
