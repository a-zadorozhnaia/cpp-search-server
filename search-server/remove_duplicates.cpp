#include "remove_duplicates.h"
#include "log_duration.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> duplicates_id;
    std::map<std::set<std::string>, int> words_to_id;

    {
        LOG_DURATION("Find");
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
        LOG_DURATION("Remove");
        for (auto id : duplicates_id) {
            search_server.RemoveDocument(id);
            std::cout << "Found duplicate document id "s << id << std::endl;
        }
    }
}

void RemoveDuplicates3(SearchServer& search_server) {
    std::set<int> duplicates_id;
    std::map<std::set<std::string>, int> words_to_id;

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
            words_to_id[words] = document_id;
        }
    }

    for (auto id : duplicates_id) {
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id "s << id << std::endl;
    }
}

void RemoveDuplicates2(SearchServer& search_server) {
    std::set<int> duplicates_id;

    {
    LOG_DURATION("Find");
    for (auto it = search_server.begin(); it != search_server.end() - 1; it++) {
        std::map<std::string, double> freq_lhs = search_server.GetWordFrequencies(*it);

        for (auto internal_it = it + 1; internal_it != search_server.end(); internal_it++) {
            std::map<std::string, double> freq_rhs = search_server.GetWordFrequencies(*internal_it);

            if (freq_lhs.size() == freq_rhs.size()) {
                bool is_dublicate = true;
                for (auto& [word, freq] : freq_lhs) {
                    if (freq_rhs.find(word) == freq_rhs.end()) {
                        is_dublicate = false;
                        break;
                    }
                }
                if (is_dublicate) {
                    duplicates_id.insert(*internal_it);
                }
            }

        }
    }
    }

    {
    LOG_DURATION("Remove");
    for (auto id : duplicates_id) {
        search_server.RemoveDocument(id);
        std::cout << "Found duplicate document id "s << id << std::endl;
    }
    }
}
