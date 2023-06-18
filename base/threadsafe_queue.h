#ifndef THREADSAFE_QUEUE_H
#define THREADSAFE_QUEUE_H

#include <mutex>
#include <condition_variable>
#include <memory> // std::move


template <typename T>
class queue_ts {
private:
    struct node {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    std::mutex head_mutex, tail_mutex;
    std::unique_ptr<node> head;
    node* tail;
    size_t m_size;
    std::condition_variable data_cond; // wait_and_pop

    node* get_tail() { // for check empty
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }

    std::unique_ptr<node> pop_head() {
        std::unique_ptr<node> old_head = std::move(head);
        --m_size;
        head = std::move(old_head->next);
        return old_head;
    }

    std::unique_lock<std::mutex> wait_for_data() {
        std::unique_lock<std::mutex> head_lock(head_mutex);
        data_cond.wait(head_lock, [&] { return head.get() != get_tail(); });
        return std::move(head_lock);
    }

    std::unique_ptr<node> wait_pop_head() {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        return pop_head();
    }

    std::unique_ptr<node> wait_pop_head(T& val) {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        val = std::move(*head->data);
        return pop_head();
    }

    std::unique_ptr<node> try_pop_head() {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail())
            return std::unique_ptr<node>();
        return pop_head();
    }

    std::unique_ptr<node> try_pop_head(T& val) {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail())
            return std::unique_ptr<node>();
        val = std::move(*head->data);
        return pop_head();
    }

public:
    queue_ts() : head(new node), tail(head.get()), m_size(0) {
    }
    queue_ts(const queue_ts& rhs) = delete;
    queue_ts& operator=(const queue_ts& rhs) = delete;

    std::shared_ptr<T> try_pop() {
        std::unique_ptr<node> old_head = pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
    std::unique_ptr<node> try_pop(T& val) {
        std::unique_ptr<node> old_head = try_pop_head(val);
        return old_head;
    }
    std::shared_ptr<T> wait_and_pop() {
        std::unique_ptr<node> const old_head = wait_pop_head();
        return old_head->data;
    }
    void wait_and_pop(T& val) {
        std::unique_ptr<node> const old_head = wait_pop_head(val);
    }
    void push(T new_val) {
        auto new_data(std::make_shared<T>(std::move(new_val)));
        std::unique_ptr<node> p(new node);
        { // RAII
            std::lock_guard<std::mutex> tail_lock(tail_mutex);
            ++m_size;
            tail->data = new_data;
            node* const new_tail = p.get();
            tail->next = std::move(p);
            tail = new_tail;
        }
        data_cond.notify_one(); // 通知一次
    }
    bool empty() { // not const
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return head.get() == get_tail();
    }
    size_t size() const {
        return m_size;
    }
};

#endif // !THREADSAFE_QUEUE_H
