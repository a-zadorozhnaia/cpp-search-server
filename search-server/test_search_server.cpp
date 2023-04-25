#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
#include "remove_duplicates.h"

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
