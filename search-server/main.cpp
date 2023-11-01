#include "test_search_server.h"

int main()
{
    TestParallelFindTopDocumentsLight();
    TestParallelFindTopDocuments();
    return 0;
}
