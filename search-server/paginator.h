#pragma once

#include <iostream>
#include <vector>

template <typename Iterator>
class IteratorRange {
public:
    explicit IteratorRange(Iterator begin, Iterator end)
        : begin_(begin)
        , end_(end)
        , size_(distance(begin_, end_))
    {
    }

    Iterator begin() const {
        return begin_;
    }

    Iterator end() const {
        return end_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator begin_;
    Iterator end_;
    size_t size_;
};

template <typename Iterator>
std::ostream& operator<< (std::ostream &out, const IteratorRange<Iterator> &range)
{
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template <typename Iterator>
class Paginator {
public:
    explicit Paginator(const Iterator begin, const Iterator end, const size_t page_size) {
        auto iter = begin;
        while (iter != end) {
            if (static_cast<size_t>(distance(iter, end)) > page_size) {
                pages_.push_back(IteratorRange(iter, iter + page_size));
                iter += page_size;
            }
            else {
                pages_.push_back(IteratorRange(iter, end));
                iter = end;
            }
        }
    }

    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }

    size_t size() const {
        return pages_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}
