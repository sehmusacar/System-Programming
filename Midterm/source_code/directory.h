#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdbool.h>

// Belirtilen dosyaya belirtilen satira icerik yazar
int appendLineToFile(const char* fileNames, int lineNumber, const char* content);

// Bir dosyayi baska bir dosyaya indirir veya kopyalar
int downloadFile(const char* sourceFile, const char* destinationFile);

// Belirtilen dizindeki dosyalarin listesini dondurur
char* displayFilesInDirectory(const char *directoryName);

// Dosya isimleri dizisini serbest birakir
void releaseFileNames(char** fileNames, int fileCount);

// Belirtilen dosyadan istenen satiri okuyup dondurur
char* getFileContent(const char* fileNames, int lineNumber);

#endif // DIRECTORY_H
