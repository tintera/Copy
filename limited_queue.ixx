// limited_queue.ixx
export module limited_queue;

import <queue>;
import <stdexcept>;

// Define an exportable interface
export template <typename T>
class LimitedQueue {
public:
    LimitedQueue(std::size_t max_size);
    void push(const T& item);
    void pop();
    T& front();
    T& back();
    std::size_t size() const;
    bool empty() const;
    bool full() const;
    std::size_t capacity() const;

private:
    std::queue<T> q;
    std::size_t max_size;
};

// Implementation of LimitedQueue member functions
template <typename T>
LimitedQueue<T>::LimitedQueue(std::size_t max_sz) : max_size(max_sz) {}

template <typename T>
void LimitedQueue<T>::push(const T& item) {
    if (q.size() >= max_size) {
        throw std::overflow_error("Queue capacity exceeded");
    }
    q.push(item);
}

template <typename T>
void LimitedQueue<T>::pop() {
    if (q.empty()) {
        throw std::underflow_error("Queue is empty");
    }
    q.pop();
}

template <typename T>
T& LimitedQueue<T>::front() {
    if (q.empty()) {
        throw std::underflow_error("Queue is empty");
    }
    return q.front();
}

template <typename T>
T& LimitedQueue<T>::back() {
    if (q.empty()) {
        throw std::underflow_error("Queue is empty");
    }
    return q.back();
}

template <typename T>
std::size_t LimitedQueue<T>::size() const {
    return q.size();
}

template <typename T>
bool LimitedQueue<T>::empty() const {
    return q.empty();
}

template <typename T>
bool LimitedQueue<T>::full() const {
    return !(q.size() < capacity());
}


template <typename T>
std::size_t LimitedQueue<T>::capacity() const {
    return max_size;
}
