#include "search_server.h"

#include <string>
#include <vector>
#include <algorithm>

#include "document.h"

using namespace std::literals;

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int> &ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Попытка добавить документ с отрицательным id"s);
    }

    if (documents_.count(document_id)) {
        throw std::invalid_argument("Попытка добавить документ c id ранее добавленного документа"s);
    }

    std::vector<std::string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id]  += inv_word_count;
        document_id_to_word_freq_[document_id][word] += inv_word_count;
    }
    documents_id_.push_back(document_id);
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {

    return FindTopDocuments(raw_query,
                            [status](int document_id, DocumentStatus document_status, int rating) {
                                return document_status == status;
                            });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    std::tuple<std::vector<std::string>, DocumentStatus> matched_documents = {matched_words, documents_.at(document_id).status};
    return matched_documents;
}

std::vector<int>::const_iterator SearchServer::begin() const {
    return documents_id_.cbegin();
}

std::vector<int>::const_iterator SearchServer::end() const {
    return documents_id_.cend();
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (document_id_to_word_freq_.find(document_id) != document_id_to_word_freq_.end()) {
        return document_id_to_word_freq_.at(document_id);
    }
    static const std::map<std::string, double> empty_result;
    return empty_result;
}

void SearchServer::RemoveDocument(int document_id) {
    // Удаление из списка id документов
    auto it = std::remove(documents_id_.begin(), documents_id_.end(), document_id);
    documents_id_.erase(it);

    // Удаление из списка документов
    documents_.erase(document_id);

    // Удаление из списка соответсвия id документа частотам слов
    document_id_to_word_freq_.erase(document_id);

    // Удаление из списка соответсвия слов id документов и частотам
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    for (auto& [word, freq] : word_to_document_freqs_) {
        freq.erase(document_id);
    }
}

bool SearchServer::IsValidWord(const std::string& word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа"s);
        }

        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}


SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        if (text.size() == 1) {
            throw std::invalid_argument("Отсутствие текста после символа «минус» в поисковом запросе"s);
        }
        if (text[1] == '-') {
            throw std::invalid_argument("Наличие более чем одного «минуса» перед минус-словом"s);
        }

        is_minus = true;
        text = text.substr(1);
    }
    return {text, is_minus, IsStopWord(text)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте поискового запроса"s);
        }

        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
