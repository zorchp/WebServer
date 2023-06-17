#ifndef THREADSAFE_QUEUE_H
#define THREADSAFE_QUEUE_H

#include "../locker/locker.hpp"


template <typename T> // http_conn*
class queue_ts {
private:
    struct node { // based on linked list
        T data;
        node* next;
    };
    locker head_mutex, tail_mutex;
    node* head;
    node* tail;
    sem data_cond; // wait_and_pop
    size_t m_size;

    node* get_tail() { // for check empty
        lock_guard tail_lock(tail_mutex);
        return tail;
    }

    node* pop_head() {
        node* old_head = head;
        head = old_head->next;
        --m_size;
        return old_head;
    }

    locker wait_for_data() {
        locker head_lock(head_mutex);
        while (empty())
            data_cond.wait();
        return head_lock;
    }

    node* wait_pop_head() {
        locker head_lock(wait_for_data());
        return pop_head();
    }

    node* wait_pop_head(T& val) {
        locker head_lock(wait_for_data());
        val = head->data;
        return pop_head();
    }

    node* try_pop_head() {
        lock_guard head_lock(head_mutex);
        if (empty())
            return nullptr;
        return pop_head();
    }

    node* try_pop_head(T& val) {
        lock_guard head_lock(head_mutex);
        if (empty())
            return nullptr;
        val = head->data;
        return pop_head();
    }

public:
    queue_ts() : head(new node), tail(head), m_size(0) {
    }
    queue_ts(const queue_ts& rhs) = delete;
    queue_ts& operator=(const queue_ts& rhs) = delete;

    T try_pop() {
        node* old_head = pop_head();
        return old_head ? old_head->data : nullptr;
    }
    node* try_pop(T& val) {
        node* old_head = try_pop_head(val);
        return old_head;
    }
    T wait_and_pop() {
        node* old_head = wait_pop_head();
        return old_head->data;
    }
    void wait_and_pop(T& val) { //
        node* old_head = wait_pop_head(val);
    }
    void push(T new_val) {
        node* p = new node;
        auto new_data = new T(new_val);
        { // RAII
            lock_guard tail_lock(tail_mutex);
            tail->data = *new_data;
            node* const new_tail = p;
            tail->next = p;
            tail = new_tail;
            ++m_size;
        }
        data_cond.post();
    }
    bool empty() { // not const
        lock_guard head_lock(head_mutex);
        return head == get_tail();
    }
    size_t size() const {
        // printf("queue size = %ld\n", m_size);
        return m_size;
    }
};

#endif // !THREADSAFE_QUEUE_H
