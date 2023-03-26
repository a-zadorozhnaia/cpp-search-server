#include "request_queue.h"
#include "document.h"
#include "search_server.h"

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server) {

}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentStatus status) {
    std::vector<Document> find_result = search_server_.FindTopDocuments(raw_query, status);
    AddQueryResult(raw_query, find_result.empty());
    return find_result;
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query) {
    std::vector<Document> find_result = search_server_.FindTopDocuments(raw_query);
    AddQueryResult(raw_query, find_result.empty());
    return find_result;
}

int RequestQueue::GetNoResultRequests() const {
    return count_if(requests_.begin(), requests_.end(), [](auto request) {return request.empty_result;});
}

void RequestQueue::AddQueryResult(const std::string request, const bool empty_result) {
    int timestamp = 1;
    if (requests_.empty()) {
        requests_.push_back({request, empty_result, timestamp});
    }
    else {
        timestamp = requests_.back().timestamp;
        if (++timestamp >= min_in_day_) {
            timestamp -= min_in_day_;
        }
        if (requests_.size() >= min_in_day_) {
            requests_.pop_front();
        }
        requests_.push_back({request, empty_result, timestamp});
    }
}
