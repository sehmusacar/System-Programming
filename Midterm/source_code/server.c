#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include "Queue.h"
#include <semaphore.h>
#include <sys/stat.h>
#include "directory.h"
#include <stdbool.h>
#include <signal.h>
#define MAX_BUF_SIZE 1024
#define MAX_WORDS 100
#define MAX_LENGTH 100
#define SHARED_MEMORY_KEY 125
#define SEMAPHORE_KEY 5678
#define MAX_SIZE 100
#define SHM_SIZE sizeof(int) + sizeof(Queue) + sizeof(SharedSemaphores) + sizeof(int)
#define MAX_FILENAME_LENGTH 248

int pid_array[1000];
bool signal_received;
bool signal_received_child;

void childWork(){

}

typedef struct {
    Queue* queue;
    
} SharedData;

typedef struct {
    sem_t semaphore;
    sem_t semaphore_for_queue_clients;
    int count;
    int currentCount;
} SharedSemaphores;

// Zaman damgasiyla birlikte log dosyasina mesaj yazar
void logMessage(const char* message) {
    time_t rawtime;
    struct tm* timeinfo;
    char timestamp[20];

    // Su anki zamani al
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    // Zamani bicimlendir
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

    // Log dosyasini ac
    FILE* logFile = fopen("log.txt", "a");
    if (logFile == NULL) {
        printf("Log dosyasi acilamadi!");
        return;
    }

    // Zaman bilgisi ve mesaji log dosyasina yaz
    fprintf(logFile, "[%s] %s\n", timestamp, message);

    // Log dosyasini kapat
    fclose(logFile);
}


void compressFile(const char *originalFilename) {
    char baseFilename[MAX_FILENAME_LENGTH + 1];

    // Dosya adından uzantıyı ayır (nokta karakterine kadar olan kısmı al)
    strncpy(baseFilename, originalFilename, MAX_FILENAME_LENGTH);
    baseFilename[MAX_FILENAME_LENGTH] = '\0';  // Null termination garantile
    char *dot = strrchr(baseFilename, '.');
    if (dot) {
        *dot = '\0';  // Nokta karakterinden itibaren kes, böylece uzantıyı kaldır
    }

    char archiveName[MAX_FILENAME_LENGTH + 8];  // ".tar.gz" ve null için ekstra yer
    snprintf(archiveName, sizeof(archiveName), "%s.tar.gz", baseFilename);  // Yeni arşiv adını güvenli şekilde oluştur

    char command[512];
    snprintf(command, sizeof(command), "tar -czf %s %s", archiveName, originalFilename);  // Tar komutunu güvenli şekilde oluştur
    system(command);  // Sistem komutunu çalıştırarak dosyayı tar ile sıkıştırır
}



// Girdi stringini kelimelere ayirir ve kelime dizisi dondurur
char** splitString(const char* inputString, int* wordCount) {
    char** tokens = (char**)malloc(MAX_WORDS * sizeof(char*));
    if (inputString == NULL) {
        *wordCount = 0;
        return NULL;
    }

    char* tempString = (char*)malloc((strlen(inputString) + 1) * sizeof(char));
    if (tempString == NULL) {
        return NULL;
    }
    strcpy(tempString, inputString);

    char* token = strtok(tempString, " ");
    int count = 0;
    while (token != NULL && count < MAX_WORDS) {
        tokens[count] = (char*)malloc((strlen(token) + 1) * sizeof(char));
        if (tokens[count] == NULL) {
            // Eğer bellek ayrılamazsa önceki tüm ayrılmış bellekleri serbest bırak
            for (int i = 0; i < count; i++) {
                free(tokens[i]);
            }
            free(tokens);
            free(tempString);
            return NULL;
        }
        strcpy(tokens[count], token);
        count++;
        token = strtok(NULL, " ");
    }
    *wordCount = count;
    free(tempString);  // tempString belleğini serbest bırak
    return tokens;
}


// Komut yurutme islevi, verilen komutlara gore cesitli dosya islemleri gerceklestirir
char* processCommand(char *command, char *dirname, char *dirnameOfClient){
    int wordCount;
    char** words = splitString(command, &wordCount); // Girdi komutunu kelimelere ayirir

    // "list" komutu, belirtilen dizindeki dosyalari listeler
    if(strcmp(words[0],"list\n") == 0 || strcmp(words[0],"list") == 0){
        for (int i = 0; i < wordCount; i++) {
            free(words[i]); // Ayirdigimiz bellegi serbest birak
        }
        free(words);
        char *listFiles = displayFilesInDirectory(dirname); // Dizini listele
        if(strlen(listFiles) == 0){
            free(listFiles);
            char* emptyString = (char*)malloc(30 * sizeof(char));
            strcpy(emptyString,"List is empty"); // Liste bossa uygun mesaji dondur
            return emptyString;
        }
        return listFiles;
    }
     // "read" komutu, belirtilen dosyadan belirli bir satiri okur
    if(strcmp(words[0], "read\n") == 0 || strcmp(words[0],"read") == 0){
        char fileNameOfServer[256]; // Dosya yolu icin bellek ayirma
        strcpy(fileNameOfServer,dirname); // Dosya adinin basina dizin yolu ekler
        strcat(fileNameOfServer,words[1]); // Dosya adini bellege ekler
        if(wordCount > 2){
            int count = atoi(words[2]); // Satir numarasini alir
            for (int i = 0; i < wordCount; i++) {
                free(words[i]); // Bellegi temizler
            }
            free(words);
            return getFileContent(fileNameOfServer,count); // Belirtilen satiri getirir
        }
        for (int i = 0; i < wordCount; i++) {
            free(words[i]);
        }
        return getFileContent(fileNameOfServer,0); // Dosyanin tamamini okur
    }
    // "write" komutu, belirtilen dosyaya icerik yazar
    if(strcmp(words[0], "write\n") == 0 || strcmp(words[0], "write") == 0){
        char fileNameOfServer[256];
        strcpy(fileNameOfServer,dirname); // Dosya yolu
        strcat(fileNameOfServer,words[1]); // Dosya adi
        int resultOfWrite; // Yazma islem sonucu
        int lineNumber; // Yazilacak satir numarasi
        char *result = malloc (sizeof (char) * 100); // Sonuc mesaji icin bellek ayirma
        char str[1000] = ""; // Yazilacak icerigi tutma
        // İcerik ve satir numarasini ayarlama
        if(words[2][0] >= '0' && words[2][0] <= '9'){
            lineNumber = atoi(words[2]); // Satir numarasi
            for(int i=3;i<wordCount;i++){
                strcat(str,words[i]); // Kelimeleri birlestir ve str'ye ekle
                strcat(str," "); // Bosluk ekleyerek kelimeleri ayir
            }
        }
        else{
            lineNumber = 0;
            for(int i=2;i<wordCount;i++){
                strcat(str,words[i]); // Kelimeleri birlestir ve str'ye ekle
                strcat(str," "); // Bosluk ekleyerek kelimeleri ayir
            } 
        }

        // Dosyaya yazma islemi gerceklestir
        if(wordCount > 3)
            resultOfWrite = appendLineToFile(fileNameOfServer, lineNumber, str);
        else
            resultOfWrite = appendLineToFile(fileNameOfServer, 0, str);
        switch (resultOfWrite) {
        case 1:
            snprintf(result,256,"Content successfully written to line %d of the file.\n", lineNumber);
            break;
        case 2:
            snprintf(result,256,"Content successfully written to the end of the file.\n");
            break;
        case 3:
            snprintf(result,256,"File created and content successfully written.\n");
            break;
        default:
            snprintf(result,256,"Failed to write content to the file.\n");
            break;
        }

        // Kelime dizisini serbest birak
        for (int i = 0; i < wordCount; i++) {
            free(words[i]);
        }
        return result;
    }
    // "download" komutu dosyayi sunucudan istemciye indirir
    if(strcmp(words[0], "download\n") == 0 || strcmp(words[0], "download") == 0){
        char fileNameOfClient[256];
        char fileNameOfServer[256];

        // İstemci ve sunucu icin dosya yollarini ayarla
        strcpy(fileNameOfClient,dirnameOfClient);
        strcat(fileNameOfClient,words[1]);
        strcpy(fileNameOfServer,dirname);
        strcat(fileNameOfServer,words[1]);

        // cocuk surecten sinyal alinirsa islemi sonlandir
        if(signal_received_child){
            printf("Signal received terminating ...\n");
            for (int i = 0; i < wordCount; i++) {
                free(words[i]); // Bellegi serbest birak
            }
            return "terminated";
        }// Dosya indirme islemini gerceklestir
        bool result = downloadFile(fileNameOfServer, fileNameOfClient);
        for (int i = 0; i < wordCount; i++) {
            free(words[i]);  // Kelimelerin bellegini serbest birak
        }
        if(result){
            return "downloaded"; // İndirme basarili
        }
        else{
            return "not downloaded"; // İndirme basarisiz
        }
    }

    if (strcmp(words[0], "archServer") == 0) {
        if (wordCount < 2) {
            for (int i = 0; i < wordCount; i++) {
                free(words[i]);
            }
            free(words);
            return "Filename not specified";
        }
        char fileNameOfClient[256], fileNameOfServer[256];
        sprintf(fileNameOfClient, "%s/%s", dirnameOfClient, words[1]);
        sprintf(fileNameOfServer, "%s/%s", dirname, words[1]);

        if (!downloadFile(fileNameOfServer, fileNameOfClient)) {
            for (int i = 0; i < wordCount; i++) {
                free(words[i]);
            }
            free(words);
            return "File download failed";
        }

        compressFile(fileNameOfClient);

        // Dosyayı sil
        unlink(fileNameOfClient);  // İndirilen dosyayı siler
        return "File archived and original deleted successfully";
    }

    // "upload" komutu dosyayi istemciden sunucuya yukler
    if(strcmp(words[0], "upload\n") == 0 || strcmp(words[0], "upload") == 0){
        char fileNameOfClient[256];
        char fileNameOfServer[256];

        // İstemci ve sunucu dosya yollarini ayarla
        strcpy(fileNameOfClient, dirnameOfClient);
        strcat(fileNameOfClient, words[1]);
        strcpy(fileNameOfServer, dirname);
        strcat(fileNameOfServer, words[1]);

        // cocuk surecten sinyal alinirsa islemi sonlandir
        if(signal_received_child){
            printf("Signal received terminating ...\n");
            for (int i = 0; i < wordCount; i++) {
                free(words[i]);  // Bellegi serbest birak
            }
            return "terminated";
        }
        // Dosya yukleme islemini gerceklestir
        int result = downloadFile(fileNameOfClient, fileNameOfServer);
        for (int i = 0; i < wordCount; i++) {
            free(words[i]);  // Kelimelerin bellegini serbest birak
        }
        // Yukleme islemi sonucuna gore cikti ver
        if(result > 0){
            char *str = malloc(sizeof(char) * 100);  // Sonuc mesaji icin bellek ayir
            snprintf(str, 100, "%d bytes transferred", result);  // Transfer edilen bayt sayisini yaz
            return str;  // Basarili transfer mesajini don
        }
        else{
            char *str = malloc(sizeof(char) * 100);  // Hata mesaji icin bellek ayir
            strcpy(str, "Can not transferred");  // Transfer edilemedi mesajini yaz
            return str;  // Basarisiz transfer mesajini don
        }        
    }
    for (int i = 0; i < wordCount; i++) {
        free(words[i]);
    }
    free(words);
    return "empty";
}

// Bagli olan her bir istemciyi isler
void manageClient(Queue* sharedQueue, char** words, int clientCount, sem_t semaphore_for_queue_clients, char *dirname, char *dirnameOfClient){
    char writeLogMessage[256];
    char fifo_path[100];

    // FIFO dosyasi olustur ve hata kontrolu yap
    sprintf(fifo_path, "/tmp/myfifo#%s", extractFromQueue(sharedQueue));
    if (mkfifo(fifo_path, 0666) == -1) {
        if (errno == EEXIST) {
            // FIFO zaten varsa hicbir sey yapma
        } else {
            perror("Error during mkfifo: "); // FIFO olusturulurken hata olustu
            return;
        }
    }
    int fd_write = open(fifo_path, O_WRONLY);

    // Istemciye baglandi bilgisini gonder
    write(fd_write, "connected", 9);
    close(fd_write);

    // Baglanti bilgisini log dosyasina yaz
    snprintf(writeLogMessage, 256, "Client PID %s connected as client%d", words[1], clientCount);
    logMessage(writeLogMessage);

    while(1){
        if(signal_received_child){
            printf("Terminating child ....\n"); // Cocuk sureci sonlandirma sinyali alindi
            break;
        }
        int fd_read = open(fifo_path, O_RDONLY);

        // FIFO'dan gelen mesajlari oku
        char buffer[1024];
        size_t num_bytes_ = read(fd_read, buffer, sizeof(buffer));
        close(fd_read);
        if(num_bytes_ == 0){
            break; // Mesaj yoksa donguden cik
        }

        // "quit" komutu geldiginde baglantiyi kes
        if(strcmp(buffer,"quit\n") == 0){
            fd_write = open(fifo_path, O_WRONLY);
            write(fd_write, "disconnected", sizeof("disconnected"));
            close(fd_write);
            break;
        }

        // Gelen komutu calistir ve sonucu geri gonder
        char *result = processCommand(buffer, dirname, dirnameOfClient);
        fd_write = open(fifo_path, O_WRONLY);
        ssize_t num_bytes_write = write(fd_write, result, strlen(result));
        close(fd_write);
        free(result);
        if(num_bytes_write == 0)
            break; // Yazma basarisizsa donguden cik
    }
}

// PID dizisini yazdirir
void displayArray(int* pid_array, int count){
    if(count == 0){
        printf("Array bos\n");  
    }
    else{
        for(int i=0; i<count; i++){
            printf("%d ", pid_array[i]);  // Dizi elemanlarini yazdirir
        }
        printf("\n");
    }
}

// Ana surec icin sinyal isleyici
void signalHandlerParent(int signum){
    for(int i = 0; i < 1000 && pid_array[i] != -1; i++){
        kill(pid_array[i], SIGINT); // Tum cocuk sureclere SIGINT sinyali gonder
    }
    signal_received = true; // Sinyal alindigini belirten bayragi guncelle
}

// cocuk surec icin sinyal isleyici
void signalHandlerChild(int signum){
    signal_received_child = true; 
}

int main(int argc, char** argv) {
    char currentDir[256] = "";
    char fifo_path[100];
    int pid = getpid();
    int maxChildCount = atoi(argv[2]); // Maksimum cocuk sayisini komut satirindan al
    char *dirname = argv[1]; // Dizin ismini komut satirindan al
    char str[MAX_BUF_SIZE];
    int sem_id;  // Semafor ID
    int localCount;
    signal_received = false;
    signal_received_child = false;

    // Sinyal isleyici ayarla
    struct sigaction sa;
    sa.sa_handler = signalHandlerParent; // Ana isleyici olarak signalHandlerParent'i ata
    sigemptyset(&sa.sa_mask); // Sinyal maskesini temizle
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) { // SIGINT icin sigaction ayarla
        perror("Sigaction hatasi"); // Hata durumunda mesaj bas
        return 1;
    }

    // Paylasilan bellek ve semaforlar icin ayarlar
    int shmid = shmget(SHARED_MEMORY_KEY, SHM_SIZE, IPC_CREAT | 0666); // Paylasilan bellek segmenti olustur
    if (shmid == -1) {
        perror("Shared memory creation error: "); // Paylasilan bellek olusturulamazsa hata bas
        return 1;
    }
    void* sharedMemory = shmat(shmid, NULL, 0); // Paylasilan bellek segmentine baglan
    if (sharedMemory == (void*) -1) {
        perror("shmat"); // Baglanti basarisizsa hata bas
        exit(1);
    }
    int* clientCount = (int*) sharedMemory; // Paylasilan bellekteki istemci sayacina isaretci
    Queue* sharedQueue = (Queue*) ((char*)sharedMemory + sizeof(int)); // Paylasilan bellekteki kuyruga isaretci
    SharedSemaphores* sharedSemaphores = (SharedSemaphores*) shmat(shmid, NULL, 0);// Paylasilan semaforlar icin bellek bolgesine baglan
    for(int i=0;i<1000;i++){
        pid_array[i] = -1; //pid yaratma
    }
    sharedSemaphores->count = 0; // Kullanilan kaynak sayisini sifirla
    sharedSemaphores->currentCount = 0; // Aktif istemci sayisini sifirla
    if (sharedSemaphores == (SharedSemaphores*) -1) {
        perror("Shared memory connection error: "); // Paylasilan bellek baglantisi hatasi
        return 1;
    }
    // SEMAFORLARI ILKLENDIR
    struct stat st; // Dizin bilgilerini saklamak icin stat yapisini kullan

    // Belirtilen dizinin var olup olmadigini kontrol et
    if (stat(dirname, &st) == -1) {
        // Dizin mevcut degilse olustur
        if (mkdir(dirname, 0777) == -1) { // Dizin olusturma hatasi
            perror("Error during on create the directory");
            return 1;
        }
    }

    strcat(dirname,"/"); // Dizin adina sona '/' karakteri ekle
    *clientCount = 0; // Musteri sayacini sifirla

    // Semafor olusturma ve baslatma islemleri
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666); // ozel semafor olustur
    if (sem_id == -1) {
        perror("Semafor olusturma hatasi"); // Semafor olusturulamazsa hata mesaji ver
        exit(EXIT_FAILURE);
    }

    // Semafor baslangic degerini ayarla
    if (semctl(sem_id, 0, SETVAL, 1) == -1) {
        perror("Semafor baslangic degeri ayarlama hatasi"); // Baslangic degeri ayarlanamazsa hata ver
        exit(EXIT_FAILURE);
    }
    sprintf(fifo_path, "/tmp/myfifo#%d", pid); // FIFO yolu olustur

    printf("Server started PID %d %s\n", pid, fifo_path); // Server basladi bilgisini yazdir
    if (mkfifo(fifo_path, 0666) == -1) { // FIFO dosyasi olustur
        if (errno == EEXIST) {
            // FIFO zaten varsa ekstra islem yapmaya gerek yok
        } else {
            perror("Error during mkfifo: "); // FIFO olusturulamazsa hata mesaji ver
            return -1;
        }
    }

    int fd_read;
    // İki semafor baslat: biri genel, digeri kuyruk istemcileri icin
    sem_init(&(sharedSemaphores->semaphore), 1, 1); // Genel semaforu baslat
    sem_init(&(sharedSemaphores->semaphore_for_queue_clients), 1, maxChildCount); // Kuyruk istemcileri icin semaforu baslat

    // Surekli calisan dongu
    while (1) {
        if(signal_received){
            printf("Signal received terminating ...\n"); // Sinyal alindi, sonlandiriliyor
            break;
        }
        fd_read = open(fifo_path, O_RDONLY); // FIFO dosyasini okuma modunda ac
        size_t num_bytes = read(fd_read, str, sizeof(str)); // FIFO'dan veri oku
        if(signal_received){
            printf("Signal received terminating ...\n"); // Sinyal alindi, sonlandiriliyor
            break;
        }
        int s_val, s_val2;
        sem_getvalue(&sharedSemaphores->semaphore_for_queue_clients, &s_val); // Kuyruk istemcileri semaforunun degerini al
        sem_getvalue(&sharedSemaphores->semaphore, &s_val2); // Genel semaforun degerini al
        close(fd_read); // FIFO dosyasini kapat
        if(num_bytes == 0){
            break; // Veri yoksa donguyu kir
        }
        int wordCount;
        if(signal_received){
            printf("Signal received terminating ...\n"); // Sinyal alindi, sonlandiriliyor
            break;
        }
        if(strcmp(str, "killServer\n") == 0 || strcmp(str, "killServer") == 0){
            signalHandlerParent(0); // killServer komutu gelirse sunucuyu kapat
            break;
        }
        char** words = splitString(str, &wordCount); // Gelen stringi kelime kelime ayir
        
        for(int i = 3; i < wordCount; i++) {
            strcat(words[2], " ");  // İki kelime arasina bosluk ekle
            strcat(words[2], words[i]);  // İkinci kelimeye sonraki kelimeleri ekle
        }
        strcat(words[2], "/");  // Sonuna '/' isareti ekle

        // Gelen komutlara gore islem yap
        if(strcmp(words[0], "connect") == 0 || strcmp(words[0], "tryConnect") == 0) {
            // Baglanti talebi isle
            if(sharedSemaphores->currentCount < maxChildCount) {
                // Maksimum cocuk sayisindan az ise yeni cocuk sureci baslat
                strcpy(currentDir, words[2]);  // Gecerli dizini ayarla
                localCount = sharedSemaphores->count++;  // Yerel sayaci artir
                sharedSemaphores->currentCount++;  // Ortak sayaci artir
                sem_wait(&sharedSemaphores->semaphore);  // Semaforu kilitle
                insertInQueue(sharedQueue, words[1]);  // Kuyruga yeni PID ekle
                sem_post(&sharedSemaphores->semaphore);  // Semafor kilidini ac
                (*clientCount)++;  // Musteri sayisini artir
                pid_t pid = fork();  // Yeni cocuk sureci baslat
                if(pid == -1) {
                    perror("Error on during fork: ");  
                    return -1;
                }
                if(pid == 0) {
                    // cocuk sureci icin sinyal isleyici ayarla
                    sa.sa_handler = signalHandlerChild; // signalHandlerChild fonksiyonunu sinyal isleyici olarak ayarla
                    sigemptyset(&sa.sa_mask); // Tum sinyallerin maskesini temizle
                    sa.sa_flags = 0; // Sinyal isleyici icin hicbir bayrak ayarlanmamis
                    if (sigaction(SIGINT, &sa, NULL) == -1) { // SIGINT icin sinyal isleyicisini ayarla
                        perror("Sigaction hatasi"); // Sigaction islemi basarisiz olursa hata mesaji ver
                        return 1; // Hata durumunda 1 donerek fonksiyondan cik
                    }

                    // Kuyruktan musteri yonetimi
                    sem_wait(&sharedSemaphores->semaphore_for_queue_clients); // Musteri kuyrugu icin semaforu kilitle
                    manageClient(sharedQueue, words, localCount, sharedSemaphores->semaphore_for_queue_clients, dirname, currentDir); // Musteri istegini isle
                    showQueue(sharedQueue); // Guncel kuyruk durumunu yazdir
                    sem_post(&sharedSemaphores->semaphore_for_queue_clients); // İslem bittiginde semaforu serbest birak
                    (*clientCount)--;  // Musteri sayisini azalt
                    for (int i = 0; i < wordCount; i++) {
                        free(words[i]);  // Kelimeleri serbest birak
                    }
                    free(words);
                    sharedSemaphores->currentCount--;  // Ortak sayaci azalt
                    printf("client%d disconnected..\n", localCount);  // Baglanti kesildi bilgisi
                    exit(0);  // cocuk sureci sonlandir
                } else {
                    // Ebeveyn sureci
                    pid_array[(sharedSemaphores->count)-1] = pid;  // PID dizisine yeni PID ekle
                }
            } else {
                // Baglanti kuyrugu dolu ise
                char writeLogMessage[256];
                if(strcmp(words[0], "connect") == 0) {
                    snprintf(writeLogMessage, 256, "Connection request PID %s... Queue FULL", words[1]);
                    logMessage(writeLogMessage);
                    insertInQueue(sharedQueue, words[1]);  // Kuyruga PID ekle
                    pid_t pid = fork();
                    if(pid == -1) {
                        perror("Error on during create child process: ");
                        return -1;
                    }
                    if(pid == 0) {
                        // cocuk sureci icin semafor beklet
                        strcpy(currentDir, words[2]); // Mevcut dizini komut dizgesinden alinan yola ayarla
                        int sem_val;
                        sem_getvalue(&sharedSemaphores->semaphore_for_queue_clients, &sem_val); // Kuyruk istemcileri icin semafor degerini al
                        sem_wait(&sharedSemaphores->semaphore_for_queue_clients); // İlgili semafor uzerinde beklemeye basla (kritik bolgeye giris)

                        localCount = sharedSemaphores->count++; // Yerel sayaci artir ve degeri localCount'a ata
                        sharedSemaphores->currentCount++; // Ortak sayaci artir

                        manageClient(sharedQueue, words, localCount, sharedSemaphores->semaphore_for_queue_clients, dirname, currentDir); // İstemciyi isleme fonksiyonunu cagir

                        sem_post(&sharedSemaphores->semaphore_for_queue_clients); // İslemler tamamlandiginda semaforu serbest birak (kritik bolgeden cikis)

                        for (int i = 0; i < wordCount; i++) { 
                            free(words[i]); // İslenen kelimelerin bellegini serbest birak
                        }
                        free(words); // Kelimeler dizisinin kendisini serbest birak

                        sharedSemaphores->currentCount--; // Ortak sayaci azalt

                        printf("client%d disconnected..\n", localCount); // İstemcinin baglantisinin kesildigini bildir
                        exit(0); // cocuk sureci sonlandir
                    }
                } else {
                    // Eger baska bir komut gelirse, FIFO dosyasini olustur
                    sprintf(fifo_path, "/tmp/myfifo#%s", words[1]);
                    if (mkfifo(fifo_path, 0666) == -1) {
                        if (errno == EEXIST) {
                            // Eger FIFO zaten varsa, ek bir islem yapilmasina gerek yok
                        } else {
                            perror("Error during mkfifo: "); // FIFO olusturulurken bir hata olustu
                            return -1;
                        }
                    }
                    int fd_write = open(fifo_path, O_WRONLY); // FIFO dosyasini yazma modunda ac
                    write(fd_write, "disconnected", 13); // İstemciye 'baglanti kesildi' mesajini gonder
                    close(fd_write); // FIFO dosyasini kapat
                }
            }
        } else if(strcmp(words[0], "tryConnect") == 0) {
            // "tryConnect" icin ayri bir durum isleme
        }

        for (int i = 0; i < wordCount; i++) {
            free(words[i]);  // Kelimelerin bellegi serbest birak
        }
        free(words);
    }
    if(fd_read != -1)
        close(fd_read);  // Okuma dosyasini kapat
    unlink(fifo_path);  // FIFO dosyasini sil
    shmdt(sharedMemory);  // Paylasilan bellegi serbest birak
    return 0;
}