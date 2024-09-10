#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>

// Bir dosyayi baska bir dosyaya kopyalar
int downloadFile(char* inputfile,char* outputfile) {
    FILE *input, *output;
    char c;
    int length = strlen(inputfile);
    int byteCount = 0;
    if (length > 0) {
        inputfile[length - 1] = '\0'; // Son karakteri null karakteriyle degistir
    }
    length = strlen(outputfile);
    if (length > 0) {
        outputfile[length - 1] = '\0'; // Son karakteri null karakteriyle degistir
    }
    
    input = fopen(inputfile, "rb+"); // Giris dosyasini okuma/yazma modunda acar
    output = fopen(outputfile, "wb+"); // Cikis dosyasini yazma/olusturma modunda acar
    
    if (input == NULL) {
        printf("INPUT Error opening file.");
        return -1;
    }
    if (output == NULL) {
        printf("OUTPUT Error opening file.");
        fclose(input);
        return -1;
    }
    
    // Dosyalari kilitle
    struct flock inputLock, outputLock; // File lock yapilari tanimlama
    inputLock.l_type = F_WRLCK; // Yazma kilidi tipi ayarlama
    inputLock.l_whence = SEEK_SET; // Dosyanin basindan itibaren
    inputLock.l_start = 0; // Kilidin baslangic noktasi
    inputLock.l_len = 0; // Kilidin uzunlugu, 0 tum dosya
    outputLock = inputLock; // Cikis dosyasi icin de ayni kilidi ayarlar

    // Kilitleme islemi yapilir
    if (fcntl(fileno(input), F_SETLK, &inputLock) == -1 || fcntl(fileno(output), F_SETLK, &outputLock) == -1) {
        printf("Error locking files.\n");
        fclose(input);
        fclose(output);
        return -1;
    }

    while (fread(&c, sizeof(char), 1, input) == 1) {
        fwrite(&c, sizeof(char), 1, output); // Her karakteri giris dosyasindan cikis dosyasina yazar
        byteCount++; // Yazilan bayt sayisini arttirir
}

    printf("File copied.");
    fclose(input);
    fclose(output);
    return byteCount;
}

// Dosyaya belirtilen satira icerik yazar
int appendLineToFile(const char* filename, int lineNumber, const char* content) {
    printf("FILENAME => %s\n",filename);
    FILE* file = fopen(filename, "a+");
    if (file == NULL) {
        printf("The file could not be opened: %s\n", filename);
        return 0;
    }

    if (lineNumber > 0) {
        // Belirtilen satira icerik yaz
        char** lines = NULL;
        int numLines = 0;
        char line[256];

        // Dosyadaki mevcut satirlari oku
        while (fgets(line, sizeof(line), file)) {
            char* temp = strdup(line);
            lines = realloc(lines, (numLines + 1) * sizeof(char*));
            lines[numLines++] = temp;
        }
        if (lineNumber <= numLines) {
            // Belirtilen pozisyonda satiri uzerine yaz
            fseek(file, 0, SEEK_SET);
            for (int i = 0; i < numLines; i++) {
                if (i + 1 == lineNumber) {
                    fputs(content, file);
                    fputc('\n', file);
                }
                fputs(lines[i], file);
                free(lines[i]);
            }
        } else {
            // Dosyanin sonuna icerik yaz
            fprintf(file, "%s\n", content);
        }

        free(lines);

        // Satir basariyla yazildi mi kontrol et
        if (numLines == 0)
            return 3;

        if (lineNumber <= numLines)
            return 1;
        else
            return 2;
    } else {
        // Dosyanin sonuna icerik yaz
        fprintf(file, "%s\n", content);
    }

    fclose(file);

    // Dosya olusturuldu ve icerik basariyla yazildi
    return 3;
}

// Belirtilen dosyadan istenen satiri okur
char* getFileContent(char* filename, int lineNumber) {
    int length = strlen(filename);
    if (length > 0) {
        if(filename[length-1] == '\n')
            filename[length - 1] = '\0'; // Satir sonu karakterini sil
    }
    FILE* file = fopen(filename, "r"); // Dosyayi okuma modunda ac
    
    if (file == NULL) {
        printf("Error opening file. %s\n", filename); 
        perror("Error : ");
        return NULL;
    }

    char* content = NULL;
    size_t contentSize = 0;

    if (lineNumber <= 0) {
        // Dosyanin tum icerigini oku
        fseek(file, 0, SEEK_END); // Dosyanin sonuna git
        contentSize = ftell(file); // Dosya boyutunu al
        fseek(file, 0, SEEK_SET); // Dosyanin basina don

        content = (char*)malloc(contentSize + 1); // Icerik icin bellek ayir
        if (content == NULL) {
            printf("Bellek ayirma basarisiz oldu.\n"); 
            fclose(file); 
            return NULL;
        }

        fread(content, 1, contentSize, file); // Dosyayi oku
        content[contentSize] = '\0'; // Sonlandirma karakteri ekle
    } else {
        // Belirli bir satiri oku
        char line[256];
        int currentLine = 1;

        while (fgets(line, sizeof(line), file)) { // Dosyadan satir satir oku
            if (currentLine == lineNumber) {
                contentSize = strlen(line); // Okunan satirin boyutunu al
                content = (char*)malloc(contentSize + 1); // Icerik icin bellek ayir
                if (content == NULL) {
                    printf("Bellek ayirma basarisiz oldu.\n"); // Bellek ayirma basarisizsa hata ver
                    fclose(file); 
                    return NULL;
                }
                strcpy(content, line); // Satiri icerige kopyala
                break;
            }
            currentLine++; // Satir sayisini arttir
        }
    }

    fclose(file); // Dosyayi kapat
    printf("%s\n",content); // Icerigi yazdir
    return content; // Icerigi dondur
}

// Verilen dizindeki dosyalari listeler
char* displayFilesInDirectory(const char *dirname) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(dirname); // Dizini ac
    if (dir == NULL) {
        printf("Your directory could not be opened: %s\n", dirname);
        return NULL;
    }

    int totalSize = 0; 
    while ((entry = readdir(dir)) != NULL) { // Dizindeki her bir dosya icin don
        totalSize += strlen(entry->d_name) + 2; // Dosya isim boyutlarini toplam boyuta ekle
    }
    rewinddir(dir); // Dizini tekrar okumak icin basa sar
    char* filenames = (char*)malloc(totalSize * sizeof(char)); // Dosya isimlerini saklamak icin bellek ayir
    if (filenames == NULL) {
        printf("Memory error\n");
        return NULL;
    }

    int offset = 0; // Dosya isimlerini eklerken kullanilacak ofset
    while ((entry = readdir(dir)) != NULL) { // Dizindeki dosyalari tekrar oku
        if(strcmp(entry->d_name,".") != 0 && strcmp(entry->d_name,"..") != 0) { // Ust dizinler haric
            strcpy(filenames + offset, entry->d_name); // Dosya adini string'e kopyala
            offset += strlen(entry->d_name); // Ofseti arttir
            filenames[offset] = '\n'; // Her dosya adindan sonra yeni satir karakteri ekle
            offset++;
        }
    }
    
    filenames[offset] = '\0';  

    closedir(dir);
    free(entry);
    
    return filenames;
}
