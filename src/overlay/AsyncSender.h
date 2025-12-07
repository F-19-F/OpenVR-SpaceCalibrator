#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <vector>
#include <string>

// 链接 Winsock 库
#pragma comment(lib, "ws2_32.lib")

// 定义数据包结构 (必须与之前的定义一致)
#pragma pack(push, 1)
struct SteamVRChaperoneData {
    float playAreaX;
    float playAreaZ;
    struct Point { float x, y, z; };
    Point collisionBounds[4];
    float hmdMatrix34[12];
};
#pragma pack(pop)

class AsyncSender {
public:
    AsyncSender();
    ~AsyncSender();

    // 初始化 Socket 资源
    void Start();
    
    // 停止并清理
    void Stop();

    // 对外接口：非阻塞发送
    void EnqueueData(const SteamVRChaperoneData& data);

private:
    void SenderThread();
    bool ConnectToServer(SOCKET& sock);
    bool SendAll(SOCKET sock, const void* buffer, int length);

private:
    std::thread m_workerThread;
    std::atomic<bool> m_running;
    
    // 线程安全队列
    std::queue<SteamVRChaperoneData> m_sendQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_cv;

    const char* SERVER_IP = "127.0.0.1";
    const int SERVER_PORT = 1191;
};