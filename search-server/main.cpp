#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>
#include <tuple>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
                     const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    }

    template<typename Predicate>
    vector<Document> FindTopDocuments (const string& raw_query, Predicate predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance || ((abs(lhs.relevance - rhs.relevance) < EPSILON) && lhs.rating > rhs.rating);
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments (const string& raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int document_rating) { return document_status == status; });
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        // если есть в документе есть минус-слова
        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word)) {
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    return tuple(vector<string>(), documents_.at(document_id).status);
                }
            }
        }
        // поиск плюс-слов
        vector<string> matched_plus_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word)) {
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_plus_words.push_back(word);
                }
            }
        }

        return tuple(matched_plus_words, documents_.at(document_id).status);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
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

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Predicate>
    vector<Document> FindAllDocuments(const Query& query, Predicate predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система добавляет документ
void TestAddDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        // Убеждаемся, что в поисковой системе не было документов
        ASSERT_EQUAL(server.GetDocumentCount(), 0);

        // Убеждаемся, что документ добавлен в поисковую систему
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_EQUAL(server.GetDocumentCount(), 1);
        const auto found_docs = server.FindTopDocuments("cat city"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// Тест проверяет, что поисковая система поддерживает минус-слова
void TestExcludeDocumentsWithMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слов находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat city"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же запроса но с минус-словом
    // возвращает пустой результат
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("cat -city"s).empty());
    }
}

// Тест проверяет, что поисковая система правильно находит соответствие документов поисковому запросу
void TestMatching() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Убеждаемся, что функция MatchDocument
    // возвращает все слова запроса, присутствующие в документе
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [words, _] = server.MatchDocument("cat city dog"s, doc_id);
        ASSERT(words.size() == 2);
        ASSERT_EQUAL(words[0], "cat"s);
        ASSERT_EQUAL(words[1], "city"s);
    }

    // Убеждаемся, что если есть соответствие по минус-слову функция MatchDocument
    // возвращает пустой результат
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        auto [words, _] = server.MatchDocument("cat -city dog"s, doc_id);
        ASSERT(words.empty());
    }
}

// Тест проверяет, что поисковая система правильно сортирует найденные документы по релевантности
void TestSortByRelevance() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(50, "dog on the grass with glass"s, DocumentStatus::ACTUAL, {3, 2, 1});
    server.AddDocument(44, "rat with glasses"s, DocumentStatus::ACTUAL, {4, 5, 0});
    server.AddDocument(45, "cute cow on the river"s, DocumentStatus::ACTUAL, {2, 7, 4});
    // Убеждаемся, что найденные документы отсортированы в порядке убывания релевантности
    {
        auto found_docs = server.FindTopDocuments("grass glasses river"s);
        ASSERT(found_docs.size() == 3);
        for (size_t i = 0; i + 1 < found_docs.size(); ++i) {
            ASSERT(found_docs[i].relevance > found_docs[i + 1].relevance);
        }
    }
}

// Тест проверяет, что поисковая система правильно вычисляет средний рейтинг документа при его добавлении
void TestAverageRatingCalculation()
{
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
	const int expected_rating = accumulate(ratings.begin(), ratings.end(), 0) / ratings.size();
    // Убеждаемся, что средний рейтинг документа вычислен правильно
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs[0].rating, expected_rating);
    }
}

// Тест проверяет, что поисковая система фильтрует результат поиска с помощью предиката заданного пользователем
void TestFindDocumentsPredicateFilter() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(50, "dog on the grass with glass"s, DocumentStatus::ACTUAL, {3, 2, 1});
    server.AddDocument(44, "rat with glasses"s, DocumentStatus::ACTUAL, {4, 5, 0});
    server.AddDocument(45, "cute cow on the river"s, DocumentStatus::ACTUAL, {2, 7, 4});
    // Убеждаемся, что результат поиска будет включать только документы с рейтингом больше 2
    {
        auto top_docs = server.FindTopDocuments("grass glasses river"s,
                                                [](int id, DocumentStatus status, int rating) {return rating > 2;});
        ASSERT(top_docs.size() == 2);

        ASSERT(top_docs[0].rating > 2);
        ASSERT(top_docs[1].rating > 2);
    }
}

// Тест проверяет, что поисковая система правильно ищет документы с заданным статусом
void TestFindDocumentsByStatus() {
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(50, "dog on the grass with glass"s, DocumentStatus::BANNED, {3, 2, 1});
    server.AddDocument(44, "rat with glasses"s, DocumentStatus::IRRELEVANT, {4, 5, 0});
    server.AddDocument(45, "cute cow on the river"s, DocumentStatus::BANNED, {2, 7, 4});
    // Убеждаемся, что результат поиска включает только документы со статусом DocumentStatus::BANNED
    {
        auto top_docs = server.FindTopDocuments("grass glasses river"s, DocumentStatus::BANNED);
        ASSERT(top_docs.size() == 2);

        ASSERT_EQUAL(top_docs[0].id, 45);
        ASSERT_EQUAL(top_docs[1].id, 50);
    }
}

// Тест проверяет, что релевантность найденных документов рассчитывается правильно
void TestRelevanceCalculation()
{
    SearchServer server;
    server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, {1, 2, 3});
    server.AddDocument(50, "dog on the grass with glass"s, DocumentStatus::BANNED, {3, 2, 1});
    server.AddDocument(44, "rat with glasses"s, DocumentStatus::IRRELEVANT, {4, 5, 0});
    server.AddDocument(45, "cute cow on the river"s, DocumentStatus::BANNED, {2, 7, 4});
	
    const double expected_relevance0 = 1.0 / 3.0 * log(server.GetDocumentCount() / 1.0);
    const double expected_relevance1 = 1.0 / 5.0 * log(server.GetDocumentCount() / 1.0);
    const double epsilon = 1e6;
    // Убеждаемся, что релевантность найденных документов вычисляется правильно
    {
        auto found_docs = server.FindTopDocuments("grass glasses river"s,
                                                [](int id, DocumentStatus status, int rating) {return rating > 2;});
        ASSERT(found_docs.size() == 2);
        ASSERT(fabs(found_docs[0].relevance - expected_relevance0) < epsilon);
        ASSERT(fabs(found_docs[1].relevance - expected_relevance1) < epsilon);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    TestAddDocument();
    TestExcludeStopWordsFromAddedDocumentContent();
    TestExcludeDocumentsWithMinusWords();
    TestMatching();
    TestSortByRelevance();
    TestAverageRatingCalculation();
    TestFindDocumentsPredicateFilter();
    TestFindDocumentsByStatus();
    TestRelevanceCalculation();
    //cout << "Search server testing finished"s << endl;
}

// --------- Окончание модульных тестов поисковой системы -----------

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine(), DocumentStatus::ACTUAL, {1, 2, 3, 4, 5});
    }
    return search_server;
}

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    TestSearchServer();
}
