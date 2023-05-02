#include "remove_duplicates.h"
#include "log_duration.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> duplicates_id;
    std::map<std::set<std::string>, int> words_to_id;

    {
        //LOG_DURATION("Find");
        for (const auto document_id : search_server) {
            std::map<std::string, double> curr_freq = search_server.GetWordFrequencies(document_id);
            std::set<std::string> words;
            for (const auto& [word, _] : curr_freq) {
                words.insert(word);
            }

            if (words_to_id.find(words) != words_to_id.end()) {
                duplicates_id.insert(document_id);
            }
            else {
                words_to_id.insert({words, document_id});
            }
        }
    }

    {
        //LOG_DURATION("Remove");
        for (auto id : duplicates_id) {
            search_server.RemoveDocument(id);
            std::cout << "Found duplicate document id "s << id << std::endl;
        }
    }
}
