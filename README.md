# cpp-search-server
Поисковый сервер
# Описание
Система поиска документов по ключевым словам.
Поддердивается:
- ранжирование результатов поиска;
- стоп-слова;
- минус-слова (документы, содержащие минус-слова, не будут включены в результаты поиска);
- удаление дубликатов документов.
# Исользование
```
SearchServer search_server("and with"s);
search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});
search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
auto docs = FindTopDocuments("curly nasty cat"s);
```
# Ситстемные требования
1. С++17 и выше
2. GCC (MinGW-64) 11.2.0
# Стек технологий:
1. qmake
