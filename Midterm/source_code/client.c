#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#define PATH_LEN 24


// Server'a kapatma sinyali gonderme
void send_kill_signal_to_server(char *fifo_path){
    printf("Sending kill request to server ...\n");
    int fd_write = open(fifo_path, O_RDWR); 
    write(fd_write, "killServer", sizeof("killServer"));
    close(fd_write);
}

bool signal_received; // Sinyal alindigini takip etmek icin

// Kullanici komutlarini listeler ve ilgili yardimi gosterir
bool ListTheCommands(char* command){
    if(strcmp(command,"help\n") == 0){
        printf("\tAvailable comments are :\nhelp, list, readF, writeT, upload, download, archServer, quit, killServer\n");
        return true;
    }
    else if(strcmp(command,"help readF\n") == 0){
        printf("readF <file> <line #>\n\tdisplay the #th line of the <file>, returns with an\nerror if <file> does not exists\n");
        return true;
    }
    else if(strcmp(command,"help list\n") == 0){
        printf("list\n\tsends a request to display the list of files in Servers directory.\n");
        return true;
    }
    else if(strcmp(command,"help writeT\n") == 0){
        printf("writeT <file> <line #> <string>\n\trequest to write the  content of “string” to the  #th  line the <file>, if the line # is not givenwrites to the end of file. If the file does not exists in Servers directory creates and edits thefile at the same time\n");
        return true;
    }
    else if(strcmp(command,"help upload\n") == 0){
        printf("upload <file> \nuploads the file from the current working directory of client to the Servers directory(beware of the cases no file in clients current working directory  and file with the samename on Servers side).\n");
        return true;
    }
    else if(strcmp(command,"help download\n") == 0){
        printf("download <file> \nrequest to receive <file> from Servers directory to client side.\n");
        return true;
    }
    else if(strcmp(command,"help archServer\n") == 0){
        printf("archServer\nArchives the data on the Server, compressing old logs and backup files.\n");
        return true;

    }
    else if(strcmp(command,"help quit\n") == 0){
        printf("quit\nSend write request to Server side log file and quits.\n");
        return true;
    }
    else if(strcmp(command,"help killServer\n") == 0){
        printf("killServer\nSends a kill request to the Server\n");
        return true;
    }
    return false;
}


// Sinyal isleyici fonksiyon
void signal_handler(int signum){
    signal_received = true;// Sinyal alindiginda degiskeni true yapar

}

int main(int argc, char** argv) {
    char fifo_path[100];
    char fifo_path_detail[100];
    char currentDir[256];
    int pid = atoi(argv[2]);
    char str[1024];
    
    // SET SIGNAL HANDLER
    struct sigaction sa;
    sa.sa_handler = signal_handler;// Sinyal geldiginde cagrilacak fonksiyon
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {  // Eger sigaction basarisiz olursa hata mesaji
        perror("Sigaction hatasi");
        return 1;
    }
    signal_received = false; // İlk basta sinyal alinmadi olarak baslat

    sprintf(fifo_path, "/tmp/myfifo#%d", pid);
    sprintf(fifo_path_detail, "/tmp/myfifo#%d", getpid());
    printf("Client started PID %d %s %s\n", pid, fifo_path,fifo_path_detail);

    if (mkfifo(fifo_path, 0777) == -1) {
        if (errno == EEXIST) {
            // FIFO zaten varsa bir sey yapma
        } else {
            perror("Error during mkfifo: "); // FIFO olusturma hatasi
            return -1;
        }
    }
    int fd_read = open(fifo_path, O_WRONLY);
    if (getcwd(currentDir, sizeof(currentDir)) == NULL) {
        perror("getcwd() error"); // Gecerli dizin bilgisini alamazsa hata ver
        return -1;
    }
    sprintf(str, "%s %d %s", argv[1], getpid(), currentDir);
    write(fd_read, str, sizeof(str));
    if (mkfifo(fifo_path_detail, 0666) == -1) {
        if (errno == EEXIST) {
            // FIFO zaten varsa bir sey yapma
        } else {
            perror("Error during mkfifo: "); // FIFO olusturma hatasi
            return -1;
        }
    }
    printf(">>Waiting for Que.. "); // Kuyruk bekleniyor mesaji
    int fd_write;
    while (1) {
        if(signal_received){ // Eğer sinyal alindiysa
            send_kill_signal_to_server(fifo_path); // Server'a oldurme sinyali gonder
            break;
        }

        char buffer[1024];
        fd_read = open(fifo_path_detail, O_RDONLY); // Sadece-okunur olarak FIFO ac
        memset(buffer, 0, sizeof(buffer)); // Buffer'i sifirla
        ssize_t num_bytes;
        do{
            memset(buffer, 0, sizeof(buffer)); // Her okuma isleminden once buffer'i sifirla
            num_bytes = read(fd_read, buffer, sizeof(buffer)); // FIFO'dan veri oku
            printf("%s\n",buffer); // Okunan veriyi yazdir
            if(signal_received){ // Eğer sinyal alindiysa
                send_kill_signal_to_server(fifo_path); // Server'a kapama sinyali gonder
                break;
            }
        }while(num_bytes == 1024); // 1024 boyutu kadar veri gelmeye devam ettiği surece donguye devam et
        
        close(fd_read); // FIFO dosyasini kapat
        
        if(num_bytes == 0){
            break; // Eğer okunan veri yoksa donguden cik
        }
        if(strcmp(buffer, "connected") == 0){
            printf("Connection established:\n"); // Bağlanti kurulduğunda bilgi ver
        }
        else if(strcmp(buffer, "notconnected") == 0){
            printf(">>Can not connected because the queue is full for tryConnect\n"); // Bağlanti kuyruğu dolu olduğunda bilgi ver
        }
        else if(strcmp(buffer, "disconnected") == 0){
            printf(">>Client is disconnected...\n"); // Bağlantinin kesildiği bilgisini ver
            break;
        }
        printf(">>Please enter a command : "); // Kullanicidan komut girmesini iste
        if(signal_received){ // Eğer sinyal alindiysa
            send_kill_signal_to_server(fifo_path); // Server'a kapatma sinyali gonder
            break;
        }
        fgets(buffer, 1024, stdin); // Kullanicidan komut oku
        if(strcmp(buffer,"killServer\n") == 0 || strcmp(buffer,"killServer") == 0){
            send_kill_signal_to_server(fifo_path); // "killServer" komutu geldiğinde server'i kapat
            break;
        }
        while(ListTheCommands(buffer)){ // Yardim komutlari isle
            printf(">>Please enter a command : "); // Kullanicidan yeni komut girmesini iste
            if(signal_received){ // Eğer sinyal alindiysa
                send_kill_signal_to_server(fifo_path); // Server'a kapatma sinyali gonder
                break;
            }
            fgets(buffer, 1024, stdin); // Kullanicidan komut oku
        }
        if(signal_received){ // Eğer sinyal alindiysa
            break; // Donguden cik
        }
        fd_write = open(fifo_path_detail, O_RDWR); // Okuma-yazma modunda FIFO ac
        write(fd_write, buffer, sizeof(buffer)); // Buffer'daki veriyi FIFO'ya yaz
        
        close(fd_write); // FIFO dosyasini kapat
    }
}