#include "process_queries.h"

#include <execution>

#include "search_server.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<std::vector<Document>> documents_lists(queries.size());
    std::transform(std::execution::par,
                   queries.begin(), queries.end(),
                   documents_lists.begin(),
                   [&search_server](const std::string& query){ return search_server.FindTopDocuments(query); });
    return documents_lists;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries)
{
    std::vector<Document> documents_lists;
    for (const auto& docs : ProcessQueries(search_server, queries)) {
        for (const auto doc : docs) {
            documents_lists.push_back(doc);
        }
    }
    return documents_lists;
}
