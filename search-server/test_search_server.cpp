#include <iostream>
#include <random>
#include <string>
#include <vector>
#include <execution>

#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
#include "remove_duplicates.h"
#include "process_queries.h"
#include "log_duration.h"

void TestSearchServerException()
{
    std::cout << "Исключения в конструкторе:"s << std::endl;
    try {
        SearchServer search_server("и в \x12на"s);
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка: "s << e.what() << std::endl;
    }

    try {
        SearchServer search_server("и в на"s);
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка: "s << e.what() << std::endl;
    }

    std::cout << "Исключения в AddDocument:"s << std::endl;
    try {
        SearchServer search_server("и в на"s);
        search_server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(1, "fluffy dog and fashionable collar"s, DocumentStatus::ACTUAL, {1, 2});
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка: "s << e.what() << std::endl;
    }

    try {
        SearchServer search_server("и в на"s);
        search_server.AddDocument(-1, "fluffy dog and fashionable collar"s, DocumentStatus::ACTUAL, {1, 2});
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка: "s << e.what() << std::endl;
    }

    try {
        SearchServer search_server("и в на"s);
        search_server.AddDocument(3, "big dog squaw\x12rec"s, DocumentStatus::ACTUAL, {1, 3, 2});
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка: "s << e.what() << std::endl;
    }

    std::cout << "Исключения в FindTopDocuments:"s << std::endl;
    try {
        SearchServer search_server("и в на"s);
        std::vector<Document> documents;
        search_server.FindTopDocuments("--fluffy"s);
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка: "s << e.what() << std::endl;
    }

    try {
        SearchServer search_server("и в на"s);
        std::vector<Document> documents;
        search_server.FindTopDocuments("-flu \ffy"s);
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка: "s << e.what() << std::endl;
    }

    try {
        SearchServer search_server("и в на"s);
        std::vector<Document> documents;
        search_server.FindTopDocuments("- fluffy"s);
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка: "s << e.what() << std::endl;
    }

//    std::cout << "Исключения в GetDocumentId:"s << std::endl;
//    try {
//        SearchServer search_server("и в на"s);
//        std::vector<Document> documents;
//        search_server.GetDocumentId(search_server.GetDocumentCount());
//    } catch (const std::out_of_range& e) {
//        std::cout << "Ошибка: "s << e.what() << std::endl;
//    }
}

void TestPaginateSearchServer()
{
    SearchServer search_server("and with"s);
    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, {1, 1, 1});
    const auto search_results = search_server.FindTopDocuments("curly dog"s);
    int page_size = 2;
    const auto pages = Paginate(search_results, page_size);
    // Выводим найденные документы по страницам
    for (auto page = pages.begin(); page != pages.end(); ++page) {
        std::cout << *page << std::endl;
        std::cout << "Page break"s << std::endl;
    }
}

int TestRequestQueue() {
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "curly dog and fancy collar"s, DocumentStatus::ACTUAL, {1, 2, 3});
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, {1, 2, 8});
    search_server.AddDocument(4, "big dog sparrow Eugene"s, DocumentStatus::ACTUAL, {1, 3, 2});
    search_server.AddDocument(5, "big dog sparrow Vasiliy"s, DocumentStatus::ACTUAL, {1, 1, 1});
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }

    // все еще 1439 запросов с нулевым результатом
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;
    request_queue.AddFindRequest("curly dog"s);

    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;
    request_queue.AddFindRequest("big collar"s);

    // первый запрос удален, 1437 запросов с нулевым результатом
    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;
    request_queue.AddFindRequest("sparrow"s);

    std::cout << "Total empty requests: "s << request_queue.GetNoResultRequests() << std::endl;
    return 0;
}

void TestRemoveDublicate() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // дубликат документа 2, будет удалён
    search_server.AddDocument(3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // отличие только в стоп-словах, считаем дубликатом
    search_server.AddDocument(4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, считаем дубликатом документа 1
    search_server.AddDocument(5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // добавились новые слова, дубликатом не является
    search_server.AddDocument(6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    search_server.AddDocument(7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});

    // есть не все слова, не является дубликатом
    search_server.AddDocument(8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

    // слова из разных документов, не является дубликатом
    search_server.AddDocument(9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

//    for (const int document_id : search_server) {
//        std::cout << "document_id = "s << document_id << std::endl;
//    }
//    std::cout << std::endl;

//    for (const auto& [word, freq] : search_server.GetWordFrequencies(9)) {
//        std::cout << word << ": "s << freq << std::endl;
//    }

    std::cout << "Before duplicates removed: "s << search_server.GetDocumentCount() << std::endl;
    RemoveDuplicates(search_server);
    std::cout << "After duplicates removed: "s << search_server.GetDocumentCount() << std::endl;
}

using namespace std;
string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}
vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}
string GenerateQueryForMatch(mt19937& generator, const vector<string>& dictionary, int max_word_count) {
    const int word_count = uniform_int_distribution(1, max_word_count)(generator);
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}
string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0) {
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}
vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}
template <typename QueriesProcessor>
void Test(string_view mark, QueriesProcessor processor, const SearchServer& search_server, const vector<string>& queries) {
    LOG_DURATION(mark);
    const auto documents_lists = processor(search_server, queries);
}
#define TEST(processor) Test(#processor, processor, search_server, queries)

void TestProcessQueiries()
{
    mt19937 generator;
    const auto dictionary = GenerateDictionary(generator, 2'000, 25);
    const auto documents = GenerateQueries(generator, dictionary, 20'000, 10);
    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }
    const auto queries = GenerateQueries(generator, dictionary, 2'000, 7);
    TEST(ProcessQueries);
}

void TestProcessQueiriesLight()
{
    SearchServer search_server("and with"s);
    int id = 0;
    for (
        const std::string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }
    const std::vector<std::string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };
    id = 0;
    for (
        const auto& documents : ProcessQueries(search_server, queries)
    ) {
        std::cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << std::endl;
    }
}

void TestProcessQueriesJoined()
{
    SearchServer search_server("and with"s);
    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }
    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };
    for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
        cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
    }
}

template <typename ExecutionPolicy>
void Test(string_view mark, SearchServer search_server, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    for (int id = 0; id < document_count; ++id) {
        search_server.RemoveDocument(policy, id);
    }
    cout << search_server.GetDocumentCount() << endl;
}

#define TEST_PAR(mode) Test(#mode, search_server, execution::mode)

void TestParallelRemoveDocuments() {
    mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 10'000, 25);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 100);

    {
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        TEST_PAR(seq);
    }
    {
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }

        TEST_PAR(par);
    }
}

void TestParallelRemoveDocumentsLight() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const string query = "curly and funny"s;

    auto report = [&search_server, &query] {
        cout << search_server.GetDocumentCount() << " documents total, "s
            << search_server.FindTopDocuments(query).size() << " documents for query ["s << query << "]"s << endl;
    };

    report();
    // однопоточная версия
    search_server.RemoveDocument(5);
    report();
    // однопоточная версия
    search_server.RemoveDocument(execution::seq, 1);
    report();
    // многопоточная версия
    search_server.RemoveDocument(execution::par, 2);
    report();
}

void TestParallelMatchDocumentLight() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const string query = "curly and funny -not"s;

    {
        const auto [words, status] = search_server.MatchDocument(query, 1);
        cout << words.size() << " words for document 1"s << endl;
        // 1 words for document 1
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::seq, query, 2);
        cout << words.size() << " words for document 2"s << endl;
        // 2 words for document 2
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
        cout << words.size() << " words for document 3"s << endl;
        // 0 words for document 3
    }
}

template <typename ExecutionPolicy>
void Test(string_view mark, SearchServer search_server, const string& query, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    const int document_count = search_server.GetDocumentCount();
    int word_count = 0;
    for (int id = 0; id < document_count; ++id) {
        const auto [words, status] = search_server.MatchDocument(policy, query, id);
        word_count += words.size();
    }
    cout << word_count << endl;
}

#define TEST_MATCH(policy) Test(#policy, search_server, query, execution::policy)

void TestParallelMatchDocument() {
    mt19937 generator;

    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);

    const string query = GenerateQuery(generator, dictionary, 500, 0.1);

    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }

    TEST_MATCH(seq);
    TEST_MATCH(par);
}
