#pragma once

#include <string>
#include <map>
#include <set>
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>
#include <set>
#include <execution>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

using namespace std::literals;

const int    MAX_RESULT_DOCUMENT_COUNT  = 5;
const double EPSILON                    = 1e-6;
const int    CONCURENT_MAP_BACKET_COUNT = 200;

class SearchServer {
public:
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);

    // Добавляет документ в поисковый сервер
    void AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    // Поиск докуметов по запросу
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string_view raw_query) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query) const;

    // Возвращает количесвто документов
    int GetDocumentCount() const;
    // Возвращает список слов и статус документа с document_id, соответсвующий запросу raw_query
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::sequenced_policy& policy, const std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::execution::parallel_policy&  policy, const std::string_view raw_query, int document_id) const;
    // Возращает итератор на id первый документа
    std::set<int>::const_iterator begin() const;
    // Возращает итератор на id последнего документа
    std::set<int>::const_iterator end() const;
    // Возвращает частоту слов по id документа
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    // Удаляет документ из поискового сервера
    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy& policy, int document_id);
    void RemoveDocument(const std::execution::parallel_policy& policy, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    // Множество слов документов
    std::deque<std::string> words_storage_;
    // Множество стоп-слов
    std::set<std::string, std::less<>> stop_words_;
    // Частота слова для каждого документа
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_id_to_word_freq_;
    // Список документов
    std::map<int, DocumentData> documents_;
    // Список id документов
    std::set<int> documents_id_;

    static bool IsValidWord(std::string_view word);
    bool IsStopWord(std::string_view word) const;
    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const bool is_par, const std::string_view text) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy& policy, const Query& query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy& policy, const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord) ) {
        throw std::invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) среди стоп-слов"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template<typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy &policy, const std::string_view raw_query, DocumentPredicate document_predicate) const {
    Query query = ParseQuery(false, raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(matched_documents.begin(),
              matched_documents.end(),
              [](const Document& lhs, const Document& rhs) {
                  if (fabs(lhs.relevance - rhs.relevance) < EPSILON) {
                      return lhs.rating > rhs.rating;
                  } else {
                      return lhs.relevance > rhs.relevance;
                  }
              }
    );

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy,
                            raw_query,
                            [status](int document_id, DocumentStatus document_status, int rating) {
                                return document_status == status;
    });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(const ExecutionPolicy& policy, const std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy& policy, const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const auto word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const auto word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy& policy, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(CONCURENT_MAP_BACKET_COUNT);

    std::for_each(std::execution::par,
                  query.plus_words.begin(),
                  query.plus_words.end(),
                  [&](std::string_view word) {
                      if (word_to_document_freqs_.count(word) != 0) {
                          const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                          for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                              const auto& document_data = documents_.at(document_id);
                              if (document_predicate(document_id, document_data.status, document_data.rating)) {
                                  document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                              }
                          }
                      }
                  }
    );

    std::for_each(std::execution::par,
                  query.minus_words.begin(),
                  query.minus_words.end(),
                  [&](std::string_view word) {
                      if (word_to_document_freqs_.count(word) != 0) {
                          for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                              document_to_relevance.erase(document_id);
                          }
                      }
                  }
    );

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}
