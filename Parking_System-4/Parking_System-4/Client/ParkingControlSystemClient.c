#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <winsock2.h>
#include <time.h>

#define PORT 55555
#define BUF_SIZE 2048

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

void error_handling(const char *message);
int connect_to_server(SOCKET *sock, const char *server_ip);

int main() {
    WSADATA wsaData;
    SOCKET sock;
    parkingRequest request;
    char buffer[BUF_SIZE];
    int choice;
    char server_ip[20];
    int connection_status;

    printf("���� IP�� �Է�: ");
    scanf("%19s", server_ip);

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        error_handling("[����] WSAStartup!");

    while (1) {
        connection_status = connect_to_server(&sock, server_ip);

        if (connection_status == -1) {
            printf("[����] ���� ������ ��õ�...\n");
            Sleep(3000); // 3�� �� �ٽ� �õ�
            continue;
        }

        printf("������ ���� ����.\n");

        while (1) {
            system("cls"); // ȭ�� �ʱ�ȭ
            printf("[�޴�]\n");
            printf("1. ���� ��û\n");
            printf("2. ���� ��û\n");
            printf("3. ���� ����\n");
            printf("4. ����\n");
            printf("�Է�: ");
            scanf("%d", &choice);
            getchar(); // ���� ���� �Һ�

            if (choice == 4)
                return 0;

            memset(&request, 0, sizeof(parkingRequest)); // �ʱ�ȭ

            switch (choice) {
                case 1:
                    system("cls"); // ȭ�� �ʱ�ȭ
                    printf("[���� ��û]\n");
                    // ���� ���� ���� Ȯ��	
                    request.request_type = 2;
                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, buffer, BUF_SIZE, 0) == SOCKET_ERROR) break;
                    printf("�� ���� ��Ȳ\n%s\n", buffer);

                    // ���� ������ �� �� ��� ���� �Ұ�
                    if (strstr(buffer, "��� ����") == NULL) {
                        printf("[�˸�] ��� �ڸ��� �����Ǿ��ֽ��ϴ�.\n������ �Ұ����մϴ�..\n");
                        system("pause");
                        break;
                    }

                    // ���� ��û
                    printf("���� ��ȣ: ");
                    fgets(request.car_platenum, sizeof(request.car_platenum), stdin);
                    request.car_platenum[strcspn(request.car_platenum, "\n")] = '\0';

                    printf("���� �ڸ�: ");
                    scanf("%d", &request.slot_id);
                    getchar(); 
                    if (request.slot_id < 1 || request.slot_id > 8) {
                        printf("[����] �߸��� ���� �ڸ� ��ȣ. 1~8 ���� ��ȣ�� �Է�.\n");
                        system("pause");
                        break;
                    }

                    printf("���� �ð� (�ð� ����): ");
                    scanf("%d", &request.parking_duration);
                    request.parking_time = time(NULL);
                    request.request_type = 0;

                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;

                    if (request.slot_id != -1) {
                        printf("[�˸�] ���� Ȯ��, ���� �ڸ� ��ȣ: %d\n", request.slot_id);
                    } else {
                        printf("[�˸�] ���� �ڸ� ���Ұ�.\n");
                    }
                    system("pause");
                    break;

                case 2:
                    system("cls"); // ȭ�� �ʱ�ȭ
                    printf("[���� ��û]\n");
                    // ������ ���� ����
                    request.request_type = 2;
                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, buffer, BUF_SIZE, 0) == SOCKET_ERROR) break;
                    printf("%s\n", buffer);

                    // �����忡 ������ ���� ��� ���� �Ұ�
                    if (strstr(buffer, "���� ��") == NULL) {
                        printf("[����] �ش� ������ �������� ����.\n");
                        system("pause");
                        break; // ���� �޴��� ���ư���
                    }

                    // ���� ��û
                    printf("���� ��ȣ ");
                    fgets(request.car_platenum, sizeof(request.car_platenum), stdin);
                    request.car_platenum[strcspn(request.car_platenum, "\n")] = '\0';

                    printf("������ ������ �Է� (1-5): ");
                    scanf("%d", &request.satisfaction_score);
                    if (request.satisfaction_score < 1 || request.satisfaction_score > 5) {
                        printf("[����] 1 ~ 5�� ���� �� ������ �Է��ϼ���.\n");
                        system("pause");
                        break;
                    }

                    printf("���� ��� ���� [����: 1 / ī��: 2]: ");
                    scanf("%d", &request.payment_method);
                    if (request.payment_method < 1 || request.payment_method > 2) {
                        printf("[����] 1, 2�� �Է��ϼ���.\n");
                        system("pause");
                        break;
                    }

                    request.request_type = 1;

                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;

                    if (strcmp(request.car_platenum, "������ �������� ����") == 0) {
                        printf("[����] ������ ã�� �� ����.\n");
                    } else if (strcmp(request.car_platenum, "�����忡 ������ ����") == 0) {
                        printf("[����] �����忡 ������ ����.\n");
                    } else if (request.parking_charge != -1) {
                        printf("[����] ���� Ȯ��: �� ���: %d\n", request.parking_charge);
                        printf("[�˸�] ���� �Ϸ�.\n");
                    } else {
                        printf("������ ã�� �� ����.\n");
                    }
                    system("pause");
                    break;

                case 3:
                    system("cls"); // ȭ�� �ʱ�ȭ
                    printf("[���� ����]\n");
                    // ���� ���� ���� Ȯ�� ��û
                    request.request_type = 2;
                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, buffer, BUF_SIZE, 0) == SOCKET_ERROR) break;
                    printf("�� ���� ��Ȳ\n%s\n", buffer);
                    system("pause");
                    break;

                default:
                    printf("�߸��� �����Դϴ�.\n");
                    system("pause");
                    break;
            }
        }

        printf("[����] ���� ���� ��õ�...\n");
        closesocket(sock);
        Sleep(3000); // 3�� �� �ٽ� �õ�
    }

    WSACleanup();
    return 0;
}

void error_handling(const char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

int connect_to_server(SOCKET *sock, const char *server_ip) {
    SOCKADDR_IN servAddr;
    *sock = socket(PF_INET, SOCK_STREAM, 0);
    if (*sock == INVALID_SOCKET)
        return -1;

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = inet_addr(server_ip);
    servAddr.sin_port = htons(PORT);

    if (connect(*sock, (SOCKADDR*)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
        return -1;

    return 0;
}

