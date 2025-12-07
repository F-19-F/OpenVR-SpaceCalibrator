#include "AsyncSender.h"
#include <iostream>

AsyncSender::AsyncSender() : m_running(false) {
    // 初始化 Winsock
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

AsyncSender::~AsyncSender() {
    Stop();
    WSACleanup();
}

void AsyncSender::Start() {
    if (m_running) return;
    
    m_running = true;
    m_workerThread = std::thread(&AsyncSender::SenderThread, this);
}

void AsyncSender::Stop() {
    if (!m_running) return;

    m_running = false;
    m_cv.notify_all(); // 唤醒线程以便退出

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void AsyncSender::EnqueueData(const SteamVRChaperoneData& data) {
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        
        // 简单策略：如果队列积压太多（比如服务端挂了），丢弃旧数据，只发最新的
        // 这样可以防止内存无限增长，且 VR 只需要最新状态
        while (m_sendQueue.size() > 5) {
            m_sendQueue.pop();
        }
        
        m_sendQueue.push(data);
    }
    m_cv.notify_one(); // 唤醒发送线程
}

bool AsyncSender::ConnectToServer(SOCKET& sock) {
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    // 设置发送超时 (防止 send 卡住太久)
    DWORD timeout = 2000; // 2秒
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    // 尝试连接
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }
    
    return true;
}

bool AsyncSender::SendAll(SOCKET sock, const void* buffer, int length) {
    const char* ptr = (const char*)buffer;
    int totalSent = 0;
    
    while (totalSent < length) {
        int sent = send(sock, ptr + totalSent, length - totalSent, 0);
        if (sent == SOCKET_ERROR) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}

void AsyncSender::SenderThread() {
    SOCKET clientSocket = INVALID_SOCKET;

    while (m_running) {
        SteamVRChaperoneData currentData;
        bool hasData = false;
        // 1. 等待数据
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            hasData = false;
            m_cv.wait(lock, [this] { return !m_sendQueue.empty() || !m_running; });

            if (!m_running) break;

            if (!m_sendQueue.empty()) {
                currentData = m_sendQueue.front();
                m_sendQueue.pop();
                hasData = true;
            }
        }

        if (hasData) {
            // 2. 检查连接状态，未连接则尝试连接
            if (clientSocket == INVALID_SOCKET) {
                if (!ConnectToServer(clientSocket)) {
                    // 连接失败，简单的退避策略：丢弃本次数据，休眠一会再试
                    // 避免在服务端未启动时疯狂占用 CPU 尝试连接
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    continue; 
                }
            }
            // 3. 发送数据
            if (!SendAll(clientSocket, &currentData, sizeof(currentData))) {
                // 发送失败（可能是连接断开），关闭 Socket，下次循环重连
                closesocket(clientSocket);
                clientSocket = INVALID_SOCKET;
                // 注意：这里数据丢失了，但对于实时同步来说，丢弃旧帧是可以接受的
            }
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET;
        }
    }

    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
    }
}
static AsyncSender g_NetworkSender;
void startNetworkSender()
{
    g_NetworkSender.Start();
}
void stopNetworkSender()
{
    g_NetworkSender.Stop();
}
void SendToRemoteServer(const SteamVRChaperoneData& data)
{
    // 这一步是完全非阻塞的，仅仅是将数据放入内存队列
    g_NetworkSender.EnqueueData(data);
}