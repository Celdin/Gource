#ifndef RECUP_FILE_H
#define RECUP_FILE_H
#include <string>

class RecupFile{
public:
    RecupFile();
    int level;
    bool estUneMethode(std::string line);
    bool estUneFinMethode();
};

#endif
