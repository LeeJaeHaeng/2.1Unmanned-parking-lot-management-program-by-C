#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
#include <winsock2.h>
#include <time.h>

#define PORT 55555
#define BUF_SIZE 2048

typedef struct parkingRequest {
    int request_type; // [주차/출차] [0/1]
    char car_platenum[20]; // 번호판
    time_t parking_time; // 주차 시간
    time_t exit_time; // 출차 시간
    int parking_duration; // 주차 기간 (시간)
    int parking_charge; // 주차 요금
    int slot_id; // 주차 공간 ID
    int satisfaction_score; // 만족도 점수 (1-5)
    int payment_method; // 결제 방식 [현금 1 / 카드 2]
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

    printf("서버 IP를 입력: ");
    scanf("%19s", server_ip);

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        error_handling("[에러] WSAStartup!");

    while (1) {
        connection_status = connect_to_server(&sock, server_ip);

        if (connection_status == -1) {
            printf("[정보] 서버 연결을 재시도...\n");
            Sleep(3000); // 3초 후 다시 시도
            continue;
        }

        printf("서버에 연결 성공.\n");

        while (1) {
            system("cls"); // 화면 초기화
            printf("[메뉴]\n");
            printf("1. 주차 요청\n");
            printf("2. 출차 요청\n");
            printf("3. 주차 상태\n");
            printf("4. 종료\n");
            printf("입력: ");
            scanf("%d", &choice);
            getchar(); // 개행 문자 소비

            if (choice == 4)
                return 0;

            memset(&request, 0, sizeof(parkingRequest)); // 초기화

            switch (choice) {
                case 1:
                    system("cls"); // 화면 초기화
                    printf("[주차 요청]\n");
                    // 주차 가능 여부 확인	
                    request.request_type = 2;
                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, buffer, BUF_SIZE, 0) == SOCKET_ERROR) break;
                    printf("각 주차 현황\n%s\n", buffer);

                    // 주차 공간이 꽉 찬 경우 주차 불가
                    if (strstr(buffer, "사용 가능") == NULL) {
                        printf("[알림] 모든 자리가 주차되어있습니다.\n주차가 불가능합니다..\n");
                        system("pause");
                        break;
                    }

                    // 주차 요청
                    printf("차량 번호: ");
                    fgets(request.car_platenum, sizeof(request.car_platenum), stdin);
                    request.car_platenum[strcspn(request.car_platenum, "\n")] = '\0';

                    printf("주차 자리: ");
                    scanf("%d", &request.slot_id);
                    getchar(); 
                    if (request.slot_id < 1 || request.slot_id > 8) {
                        printf("[에러] 잘못된 주차 자리 번호. 1~8 사이 번호를 입력.\n");
                        system("pause");
                        break;
                    }

                    printf("주차 시간 (시간 단위): ");
                    scanf("%d", &request.parking_duration);
                    request.parking_time = time(NULL);
                    request.request_type = 0;

                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;

                    if (request.slot_id != -1) {
                        printf("[알림] 주차 확인, 주차 자리 번호: %d\n", request.slot_id);
                    } else {
                        printf("[알림] 주차 자리 사용불가.\n");
                    }
                    system("pause");
                    break;

                case 2:
                    system("cls"); // 화면 초기화
                    printf("[출차 요청]\n");
                    // 주차장 상태 갱신
                    request.request_type = 2;
                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, buffer, BUF_SIZE, 0) == SOCKET_ERROR) break;
                    printf("%s\n", buffer);

                    // 주차장에 차량이 없는 경우 출차 불가
                    if (strstr(buffer, "주차 중") == NULL) {
                        printf("[에러] 해당 차량이 존재하지 않음.\n");
                        system("pause");
                        break; // 메인 메뉴로 돌아가기
                    }

                    // 출차 요청
                    printf("차량 번호 ");
                    fgets(request.car_platenum, sizeof(request.car_platenum), stdin);
                    request.car_platenum[strcspn(request.car_platenum, "\n")] = '\0';

                    printf("만족도 점수를 입력 (1-5): ");
                    scanf("%d", &request.satisfaction_score);
                    if (request.satisfaction_score < 1 || request.satisfaction_score > 5) {
                        printf("[에러] 1 ~ 5점 범위 안 점수를 입력하세요.\n");
                        system("pause");
                        break;
                    }

                    printf("결제 방식 선택 [현금: 1 / 카드: 2]: ");
                    scanf("%d", &request.payment_method);
                    if (request.payment_method < 1 || request.payment_method > 2) {
                        printf("[에러] 1, 2를 입력하세요.\n");
                        system("pause");
                        break;
                    }

                    request.request_type = 1;

                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;

                    if (strcmp(request.car_platenum, "차량이 존재하지 않음") == 0) {
                        printf("[정보] 차량을 찾을 수 없음.\n");
                    } else if (strcmp(request.car_platenum, "주차장에 차량이 없음") == 0) {
                        printf("[정보] 주차장에 차량이 없음.\n");
                    } else if (request.parking_charge != -1) {
                        printf("[정보] 출차 확인: 총 요금: %d\n", request.parking_charge);
                        printf("[알림] 결제 완료.\n");
                    } else {
                        printf("차량을 찾을 수 없음.\n");
                    }
                    system("pause");
                    break;

                case 3:
                    system("cls"); // 화면 초기화
                    printf("[주차 상태]\n");
                    // 주차 공간 상태 확인 요청
                    request.request_type = 2;
                    if (send(sock, (char*)&request, sizeof(request), 0) == SOCKET_ERROR) break;
                    if (recv(sock, buffer, BUF_SIZE, 0) == SOCKET_ERROR) break;
                    printf("각 주차 현황\n%s\n", buffer);
                    system("pause");
                    break;

                default:
                    printf("잘못된 선택입니다.\n");
                    system("pause");
                    break;
            }
        }

        printf("[정보] 서버 연결 재시도...\n");
        closesocket(sock);
        Sleep(3000); // 3초 후 다시 시도
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

