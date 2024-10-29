#include <winsock2.h>
#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <time.h>

#define BUF_SIZE 2048
#define MAX_CLNT 256
#define TOTAL_SPOTS 8
#define BASE_RATE 1000
#define SHORT_TERM_RATE 500
#define SHORT_TERM_DURATION 1800 // 30��

#define NODEMCU_IP "192.168.1.175"
#define NODEMCU_PORT 80

typedef struct parkingRequest {
    int request_type; // [����/����] [0/1]
    char car_platenum[20]; // ��ȣ��
    time_t parking_time; // ���� �ð�
    time_t exit_time; // ���� �ð�
    int parking_duration; // ���� �Ⱓ (�ð�)
    int parking_charge; // ���� ���
    int slot_id; // ���� ���� ID
    int satisfaction_score; // ������ ���� (1-5)
    int payment_method; // ���� ��� [���� 1 / ī�� 2]
} parkingRequest;

typedef struct parkingStatus {
    int slot_id;
    int parking_status; // 0: �������, 1: ���� ��
    char car_platenum[20]; // ��ȣ��
    time_t parking_time; // ���� �ð�
    int parking_duration; // ���� �Ⱓ (�ð�)
} parkingStatus;

typedef struct parkingLog {
    char car_platenum[20];
    time_t parking_time;
    int parking_duration;
    time_t exit_time;
    int slot_id;
    int parking_charge;
    char action[10]; // "����" �Ǵ� "����"
    int satisfaction_score; // ������ ���� (1-5)
} parkingLog;

unsigned WINAPI HandleClient(void* arg);
void initialize_parking_spots();
int handle_parking_request(parkingRequest* request);
int handle_exit_request(parkingRequest* request);
void send_parking_status(SOCKET clientSock);
void save_parking_info();
void save_parking_log(parkingLog* log);
void update_report();
int connect_to_nodemcu(SOCKET *sock);
void send_request_to_nodemcu(SOCKET sock, const char *message);
int calculate_parking_charge(int parking_duration);

int clientCount = 0;
SOCKET clientSocks[MAX_CLNT];
HANDLE hMutex;
parkingStatus parking_spots[TOTAL_SPOTS];

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET serverSock, clientSock;
    SOCKADDR_IN serverAddr, clientAddr;
    int clientAddrSize;
    HANDLE hThread;
    int portNum;
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;

    hMutex = CreateMutex(NULL, FALSE, NULL);
    portNum = 55555;

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    serverSock = socket(PF_INET, SOCK_STREAM, 0);

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(portNum);

    bind(serverSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
    listen(serverSock, 5);

    // Get the host name
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    if (hostname == -1) {
        perror("gethostname");
        exit(1);
    }

    // Get the host information
    host_entry = gethostbyname(hostbuffer);
    if (host_entry == NULL) {
        perror("gethostbyname");
        exit(1);
    }

    // Convert the host's IP address to a string
    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));

    printf("[����] ���� �غ�, ip: %s, port: %d\n", IPbuffer, portNum);

    initialize_parking_spots();

    while (1) {
        clientAddrSize = sizeof(clientAddr);
        clientSock = accept(serverSock, (SOCKADDR*)&clientAddr, &clientAddrSize);
        
        WaitForSingleObject(hMutex, INFINITE);
        clientSocks[clientCount++] = clientSock;
        ReleaseMutex(hMutex);
        
        hThread = (HANDLE)_beginthreadex(NULL, 0, HandleClient, (void*)&clientSock, 0, NULL);
        printf("[����] ����� Ŭ���̾�Ʈ IP: %s\n", inet_ntoa(clientAddr.sin_addr));
    }

    closesocket(serverSock);
    WSACleanup();
    return 0;
}

void initialize_parking_spots() {
    int i;
    for (i = 0; i < TOTAL_SPOTS; i++) {
        parking_spots[i].slot_id = i + 1;
        parking_spots[i].parking_status = 0;
        parking_spots[i].parking_time = 0;
        parking_spots[i].parking_duration = 0;
        strcpy(parking_spots[i].car_platenum, "");
    }
}

int handle_parking_request(parkingRequest* request) {
    int i;
    parkingLog log;

    for (i = 0; i < TOTAL_SPOTS; i++) {
        if (parking_spots[i].parking_status == 0) {
            int slot_index = request->slot_id - 1;
            parking_spots[slot_index].parking_status = 1;
            parking_spots[slot_index].parking_time = time(NULL); // ���� �ð����� ���� �ð� ����
            parking_spots[slot_index].parking_duration = request->parking_duration; // ���� �Ⱓ ����
            strcpy(parking_spots[slot_index].car_platenum, request->car_platenum);
            request->parking_time = parking_spots[slot_index].parking_time;
            request->parking_charge = 0; // ���� �� ���� ��� ����

            printf("���� %s�� %d�� ���� ������ �����߽��ϴ�.\n", request->car_platenum, request->slot_id);

            // �α� ����
            strcpy(log.car_platenum, request->car_platenum);
            log.parking_time = request->parking_time;
            log.parking_duration = request->parking_duration;
            log.exit_time = 0; // ���� �� ���� �ð��� 0���� ����
            log.slot_id = request->slot_id;
            log.parking_charge = 0; // ���� �� ��� ����
            strcpy(log.action, "����");
            log.satisfaction_score = 0; // ���� �� ������ ���� ����
            save_parking_log(&log);
            
            save_parking_info();

            return parking_spots[slot_index].slot_id;
        }
    }
    return -1; // ���� ������ ����
}

int handle_exit_request(parkingRequest* request) {
    int i;
    int vehicle_found = 0;
    int parking_lot_empty = 1;
    parkingLog log;

    for (i = 0; i < TOTAL_SPOTS; i++) {
        if (parking_spots[i].parking_status == 1) {
            parking_lot_empty = 0;
            if (strcmp(parking_spots[i].car_platenum, request->car_platenum) == 0) {
                vehicle_found = 1;

                parking_spots[i].parking_status = 0;
                time_t parking_time = parking_spots[i].parking_time;
                int parking_duration = parking_spots[i].parking_duration;
                time_t exit_time = time(NULL); // ���� �ð� ����
                request->parking_time = parking_time;
                request->exit_time = exit_time;
                request->parking_duration = parking_duration;
                int charge = calculate_parking_charge(parking_duration);

                strcpy(parking_spots[i].car_platenum, "");
                request->parking_charge = charge; // ���� ���

                // �α� ����
                strcpy(log.car_platenum, request->car_platenum);
                log.parking_time = parking_time;
                log.parking_duration = parking_duration;
                log.exit_time = exit_time;
                log.slot_id = request->slot_id;
                log.parking_charge = charge;
                strcpy(log.action, "����");
                log.satisfaction_score = request->satisfaction_score; // ���� �� ������ ���� ���
                save_parking_log(&log);
                update_report(); // ���� �� ����Ʈ ����

                // ������ ���� �ݿ�
                printf("���� %s�� ������ ����: %d\n", request->car_platenum, request->satisfaction_score);
                
                save_parking_info();

                return 0; // ���� ����
            }
        }
    }

    if (parking_lot_empty) {
        return -2; // �������� �������
    } else if (!vehicle_found) {
        return -1; // ������ ã�� �� ����
    }

    return -1; // ���� �߻�
}

int calculate_parking_charge(int parking_duration) {
    if (parking_duration * 3600 <= SHORT_TERM_DURATION) {
        return SHORT_TERM_RATE; // 30�� ���� 500��
    } else {
        return parking_duration * BASE_RATE; // �ð��� 1000��
    }
}

void send_parking_status(SOCKET clientSock) {
    char status_msg[BUF_SIZE] = "";
    char temp[BUF_SIZE];
    int i;

    for (i = 0; i < TOTAL_SPOTS; i++) {
        if (parking_spots[i].parking_status) {
            sprintf(temp, "���� ��ȣ %d: ���� ��, ���� ��ȣ: %s, ���� �Ⱓ: %d �ð�\n", 
                    parking_spots[i].slot_id, parking_spots[i].car_platenum, parking_spots[i].parking_duration);
        } else {
            sprintf(temp, "���� ��ȣ %d: ��� ����\n", parking_spots[i].slot_id);
        }
        strcat(status_msg, temp);
    }
    send(clientSock, status_msg, strlen(status_msg), 0);
}

void save_parking_info() {
    FILE* file = fopen("parking_info.txt", "w");
    if (file == NULL) {
        perror("���� ���� ���� ���� ����");
        return;
    }

    int i;
    for (i = 0; i < TOTAL_SPOTS; i++) {
        fprintf(file, "���� ��ȣ %d: %s, ���� ��ȣ: %s, ���� �Ⱓ: %d �ð�\n", 
                parking_spots[i].slot_id,
                parking_spots[i].parking_status ? "���� ��" : "��� ����",
                parking_spots[i].car_platenum,
                parking_spots[i].parking_duration);
    }

    fclose(file);
}

void save_parking_log(parkingLog* log) {
    FILE* file = fopen("parking_log.txt", "a");
    if (file == NULL) {
        perror("���� ��� ���� ���� ����");
        return;
    }

    // ���� ���� ����
    char parking_time_str[26];
    char exit_time_str[26];
    strncpy(parking_time_str, ctime(&log->parking_time), 24);
    parking_time_str[24] = '\0';
    if (log->exit_time != 0) {
        strncpy(exit_time_str, ctime(&log->exit_time), 24);
        exit_time_str[24] = '\0';
    } else {
        strcpy(exit_time_str, "N/A");
    }

    if (strcmp(log->action, "����") == 0) {
        fprintf(file, "[%s] ������ȣ: %s, �����ð�: %s, ����ĭ ��ȣ: %d, ���� �Ⱓ: %d �ð�\n",
                log->action, log->car_platenum, parking_time_str, log->slot_id, log->parking_duration);
    } else {
        fprintf(file, "[%s] ������ȣ: %s, �����ð�: %s, ����ĭ ��ȣ: %d, ���� ���: %d, ������ ����: %d\n",
                log->action, log->car_platenum, exit_time_str, log->slot_id, log->parking_charge, log->satisfaction_score);
    }

    fclose(file);
}

void update_report() {
    FILE* log_file = fopen("parking_log.txt", "r");
    FILE* report_file = fopen("report.txt", "w");
    if (log_file == NULL || report_file == NULL) {
        perror("���� ���� ����");
        return;
    }

    char buffer[BUF_SIZE];
    int total_charge = 0, slot_usage[TOTAL_SPOTS] = {0};
    int exit_count = 0, total_satisfaction_score = 0, satisfaction_count = 0;
    double total_parking_duration = 0;
    char action[10], car_platenum[20];
    char parking_time_str[26], exit_time_str[26];
    int slot_id, parking_charge, parking_duration, satisfaction_score;

    while (fgets(buffer, BUF_SIZE, log_file) != NULL) {
        if (sscanf(buffer, "[%[^]]] ������ȣ: %[^,], �����ð�: %[^,], ����ĭ ��ȣ: %d, ���� �Ⱓ: %d �ð�\n",
                   action, car_platenum, parking_time_str, &slot_id, &parking_duration) == 5) {
            if (slot_id > 0 && slot_id <= TOTAL_SPOTS) {
                slot_usage[slot_id - 1]++;
                total_parking_duration += parking_duration;
            }
        } else if (sscanf(buffer, "[%[^]]] ������ȣ: %[^,], �����ð�: %[^,], ����ĭ ��ȣ: %d, ���� ���: %d, ������ ����: %d\n",
                          action, car_platenum, exit_time_str, &slot_id, &parking_charge, &satisfaction_score) == 6) {
            total_charge += parking_charge;
            exit_count++;
            total_satisfaction_score += satisfaction_score;
            satisfaction_count++;
        }
    }

    fprintf(report_file, "����Ʈ ����\n");
    fprintf(report_file, "1. ���� ���� �̿��:\n");
    for (int i = 0; i < TOTAL_SPOTS; i++) {
        fprintf(report_file, "  ���� ��ȣ %d: %d �� ����\n", i + 1, slot_usage[i]);
    }
    fprintf(report_file, "\n2. �� ����:\n");
    fprintf(report_file, "  %d ��\n", total_charge);
    fprintf(report_file, "\n3. ��� ���� �ð�:\n");
    if (exit_count > 0) {
        double average_parking_duration = total_parking_duration / exit_count;
        int hours = (int)(average_parking_duration);
        int minutes = (int)((average_parking_duration - hours) * 60);
        fprintf(report_file, "  %d�ð� %d��\n", hours, minutes);
    } else {
        fprintf(report_file, "  ������ ����\n");
    }
    fprintf(report_file, "\n4. ��� ������ ����:\n");
    if (satisfaction_count > 0) {
        double average_satisfaction_score = (double)total_satisfaction_score / satisfaction_count;
        fprintf(report_file, "  %.1f��\n", average_satisfaction_score);
    } else {
        fprintf(report_file, "  ������ ����\n");
    }

    fclose(log_file);
    fclose(report_file);
    printf("[����] ����Ʈ�� ����, ����: report.txt\n");
}

unsigned WINAPI HandleClient(void* arg) {
    SOCKET clientSock = *((SOCKET*)arg);
    int strLen = 0, i;
    parkingRequest request;

    while ((strLen = recv(clientSock, (char*)&request, sizeof(request), 0)) > 0) {
        if (request.request_type == 0) {
            // ���� ��û ó��
            int slot_id = handle_parking_request(&request);
            if (slot_id != -1) {
                send(clientSock, (char*)&request, sizeof(request), 0);
                // ������ ���������� �Ϸ�� ��쿡�� NodeMCU�� ��û�� ����
                SOCKET nodemcuSock;
                if (connect_to_nodemcu(&nodemcuSock) == 0) {
                    printf("���ܱ� ���� ����.\n");
                    send_request_to_nodemcu(nodemcuSock, "1"); // NodeMCU�� ���� ��û
                    closesocket(nodemcuSock);
                } else {
                    printf("���ܱ� ���� ����.\n");
                }
            } else {
                request.slot_id = -1;
                send(clientSock, (char*)&request, sizeof(request), 0);
            }
        } else if (request.request_type == 1) {
            // ���� ��û ó��
            int result = handle_exit_request(&request);
            if (result == 0) {
                send(clientSock, (char*)&request, sizeof(request), 0);
                // ������ ���������� �Ϸ�� ��쿡�� NodeMCU�� ��û�� ����
                SOCKET nodemcuSock;
                if (connect_to_nodemcu(&nodemcuSock) == 0) {
                    printf("���ܱ� ���� ����.\n");
                    send_request_to_nodemcu(nodemcuSock, "0"); // NodeMCU�� ���� ��û
                    closesocket(nodemcuSock);
                } else {
                    printf("���ܱ� ���� ����.\n");
                }
            } else if (result == -1) {
                strcpy(request.car_platenum, "������ �������� ����");
                request.parking_charge = -1;
                send(clientSock, (char*)&request, sizeof(request), 0);
            } else if (result == -2) {
                strcpy(request.car_platenum, "�����忡 ������ ����");
                request.parking_charge = -1;
                send(clientSock, (char*)&request, sizeof(request), 0);
            }
        } else if (request.request_type == 2) {
            // ���� ���� ���� Ȯ�� ��û
            send_parking_status(clientSock);
        }
    }

    printf("Ŭ���̾�Ʈ�� ������ �����߽��ϴ�...\n");
    WaitForSingleObject(hMutex, INFINITE);
    for (i = 0; i < clientCount; i++) {
        if (clientSock == clientSocks[i]) {
            while (i++ < clientCount - 1)
                clientSocks[i] = clientSocks[i + 1];
            break;
        }
    }
    clientCount--;
    ReleaseMutex(hMutex);
    closesocket(clientSock);
    return 0;
}

int connect_to_nodemcu(SOCKET *sock) {
    struct sockaddr_in nodemcuAddr;

    *sock = socket(PF_INET, SOCK_STREAM, 0);
    if (*sock == INVALID_SOCKET) {
        return -1;
    }

    memset(&nodemcuAddr, 0, sizeof(nodemcuAddr));
    nodemcuAddr.sin_family = AF_INET;
    nodemcuAddr.sin_addr.s_addr = inet_addr(NODEMCU_IP);
    nodemcuAddr.sin_port = htons(NODEMCU_PORT);

    if (connect(*sock, (struct sockaddr*)&nodemcuAddr, sizeof(nodemcuAddr)) == SOCKET_ERROR) {
        closesocket(*sock);
        return -1;
    }

    return 0;
}

void send_request_to_nodemcu(SOCKET sock, const char *message) {
    send(sock, message, strlen(message), 0);
    printf("���ܱ� ��� ����: %s\n", message);
}

