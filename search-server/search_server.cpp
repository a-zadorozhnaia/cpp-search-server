#include "search_server.h"

#include <string>
#include <vector>
#include <algorithm>
#include <set>
#include "document.h"

using namespace std::literals;

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

SearchServer::SearchServer(const std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, const std::string_view document, DocumentStatus status, const std::vector<int> &ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Попытка добавить документ с отрицательным id"s);
    }

    if (documents_.count(document_id)) {
        throw std::invalid_argument("Попытка добавить документ c id ранее добавленного документа"s);
    }

    words_storage_.emplace_back(document);
    std::vector<std::string_view> words = SplitIntoWordsNoStop(words_storage_.back());
    const double inv_word_count = 1.0 / words.size();
    for (const std::string_view word : words) {
        word_to_document_freqs_[word][document_id]  += inv_word_count;
        document_id_to_word_freq_[document_id][word] += inv_word_count;
    }
    documents_id_.insert(document_id);
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query, DocumentStatus status) const {

    return FindTopDocuments(raw_query,
                            [status](int document_id, DocumentStatus document_status, int rating) {
                                return document_status == status;
                            });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::sequenced_policy exec, const std::string_view raw_query, int document_id) const {
    Query query = ParseQuery(false, raw_query);
    // Если есть хоть одно минус слово, возвращаем пустой вектор matched_words
    for (const auto word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            return {std::vector<std::string_view>(), documents_.at(document_id).status};
        }
    }

    std::vector<std::string_view> matched_words;
    for (const auto word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    std::tuple<std::vector<std::string_view>, DocumentStatus> matched_documents = {matched_words, documents_.at(document_id).status};
    return matched_documents;
}

//1. Не нужно стараться написать одинаковый код для паралельной и последовательной политик.
//   Пишите разные методы - последовательная версия просто вызывает версию без политик.
//2. Использование std::set в Query не позволит эффективно распаралелить работу с плюс и минус словами.
//   В самой ParseQuery ничего распаралеливать не нужно.
//   Однако при переходе на другие контейнеры в Query нужно в той версии, которая используется без политик
//   и в последовательной политике избавиться от дубликатов плюс и минус слов.
//   Посмотрите на алгоритмы std::sort--std::unique--std::vector::erase.
//   Для паралельной версии удаление дубликатов нужно делать непосредственно в Match
//3. В паралельной и последовательной версиях Match сначала проверяйте минус слова.
//   Посмотрите std::any_of
//4. В паралельной версии при создании вектора результатов задайте его размер сразу
//   (подумайте - какой это размер может быть максимальным).
//   И используйте std::copy_if - обратите внимание на возвращаемый этим алгоритмом результат
//   - это поможет откорректировать размер вектора результатов.
std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(std::execution::parallel_policy exec, const std::string_view raw_query, int document_id) const {
    Query query = ParseQuery(true, raw_query);

    // Если есть хоть одно минус слово, возвращаем пустой вектор matched_words
    if (std::any_of(exec,
                    query.minus_words.begin(),
                    query.minus_words.end(),
                    [&](auto word)
                    {
                        return word_to_document_freqs_.at(word).count(document_id);
                    }))
    {
        return {std::vector<std::string_view>(), documents_.at(document_id).status};
    }

    // Создаём вектор совпадений максимально возможного размера
    std::vector<std::string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(exec,
                             query.plus_words.begin(),
                             query.plus_words.end(),
                             matched_words.begin(),
                             [&](const std::string_view word) {
                                return word_to_document_freqs_.at(word).count(document_id);
    });

    //удаляем дубликаты слов
    std::sort(matched_words.begin(), words_end);
    words_end = std::unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    std::tuple<std::vector<std::string_view>, DocumentStatus> matched_documents = {matched_words, documents_.at(document_id).status};
    return matched_documents;
}

std::set<int>::const_iterator SearchServer::begin() const {
    return documents_id_.cbegin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return documents_id_.cend();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    auto it = document_id_to_word_freq_.find(document_id);
    if (it != document_id_to_word_freq_.end()) {
        return it->second;
    }
    static const std::map<std::string_view, double> empty_result;
    return empty_result;
}

void SearchServer::RemoveDocument(int document_id)
{
    RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(std::execution::sequenced_policy exec, int document_id) {
    if (documents_id_.count(document_id) == 0)
        return;

    // Удаление из списка id документов
    documents_id_.erase(document_id);

    // Удаление из списка документов
    documents_.erase(document_id);

    // Удаление из списка соответсвия слов id документов и частотам
    for (auto& [word, freq] : document_id_to_word_freq_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }

    // Удаление из списка соответсвия id документа частотам слов
    document_id_to_word_freq_.erase(document_id);
}

void SearchServer::RemoveDocument(std::execution::parallel_policy exec, int document_id) {
    if (documents_id_.count(document_id) == 0)
        return;

    // Удаление из списка id документов
    documents_id_.erase(document_id);

    // Удаление из списка документов
    documents_.erase(document_id);

    std::vector<std::string_view> words_ptrs(document_id_to_word_freq_.at(document_id).size());
    // Cобираем указатели на слова, которые нужно удалить
    std::transform(std::execution::par,
                   document_id_to_word_freq_.at(document_id).begin(),
                   document_id_to_word_freq_.at(document_id).end(),
                   words_ptrs.begin(),
                   [] (const auto& pair_) {
                        const std::string_view word_ptr = pair_.first;
                        return word_ptr; });

    // Удаление из списка соответсвия слов id документов и частотам
    std::for_each(std::execution::par,
                  words_ptrs.begin(),
                  words_ptrs.end(),
                  [this, document_id] (auto& word) {
                        word_to_document_freqs_.at(word).erase(document_id);});
    // Удаление из списка соответсвия id документа частотам слов
    document_id_to_word_freq_.erase(document_id);
}

bool SearchServer::IsValidWord(std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view word : SplitIntoWords(text)) {
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


SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
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

SearchServer::Query SearchServer::ParseQuery(const bool is_par, const std::string_view text) const {
    Query query;
    for (const auto word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Наличие недопустимых символов (с кодами от 0 до 31) в тексте поискового запроса"s);
        }

        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            } else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    // Удаление дубликатов для не параллельной версии
    if (!is_par) {
        std::sort(query.minus_words.begin(), query.minus_words.end());
        auto past_the_end_it = std::unique(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(past_the_end_it, query.minus_words.end());

        std::sort(query.plus_words.begin(), query.plus_words.end());
        past_the_end_it = std::unique(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(past_the_end_it, query.plus_words.end());
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
