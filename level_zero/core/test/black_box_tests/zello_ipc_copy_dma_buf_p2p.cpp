/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>

#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

template <bool TerminateOnFailure, typename ResulT>
inline void validate(ResulT result, const char *message) {
    if (result == ZE_RESULT_SUCCESS) {
        return;
    }

    if (TerminateOnFailure) {
        std::cerr << (TerminateOnFailure ? "ERROR : " : "WARNING : ") << message << " : " << result
                  << std::endl;
        std::terminate();
    }
}

#define SUCCESS_OR_TERMINATE(CALL) validate<true>(CALL, #CALL)
#define SUCCESS_OR_TERMINATE_BOOL(FLAG) validate<true>(!(FLAG), #FLAG)
#define SUCCESS_OR_WARNING(CALL) validate<false>(CALL, #CALL)
#define SUCCESS_OR_WARNING_BOOL(FLAG) validate<false>(!(FLAG), #FLAG)

uint8_t uinitializedPattern = 1;
uint8_t expectedPattern = 7;
size_t allocSize = 4096 + 7; // +7 to break alignment and make it harder

static int sendmsg_fd(int socket, int fd) {
    char sendBuf[sizeof(ze_ipc_mem_handle_t)] = {};
    char cmsgBuf[CMSG_SPACE(sizeof(ze_ipc_mem_handle_t))];

    struct iovec msgBuffer;
    msgBuffer.iov_base = sendBuf;
    msgBuffer.iov_len = sizeof(*sendBuf);

    struct msghdr msgHeader = {};
    msgHeader.msg_iov = &msgBuffer;
    msgHeader.msg_iovlen = 1;
    msgHeader.msg_control = cmsgBuf;
    msgHeader.msg_controllen = CMSG_LEN(sizeof(fd));

    struct cmsghdr *controlHeader = CMSG_FIRSTHDR(&msgHeader);
    controlHeader->cmsg_type = SCM_RIGHTS;
    controlHeader->cmsg_level = SOL_SOCKET;
    controlHeader->cmsg_len = CMSG_LEN(sizeof(fd));

    *(int *)CMSG_DATA(controlHeader) = fd;
    ssize_t bytesSent = sendmsg(socket, &msgHeader, 0);
    if (bytesSent < 0) {
        return -1;
    }

    return 0;
}

static int recvmsg_fd(int socket) {
    int fd = -1;
    char recvBuf[sizeof(ze_ipc_mem_handle_t)] = {};
    char cmsgBuf[CMSG_SPACE(sizeof(ze_ipc_mem_handle_t))];

    struct iovec msgBuffer;
    msgBuffer.iov_base = recvBuf;
    msgBuffer.iov_len = sizeof(recvBuf);

    struct msghdr msgHeader = {};
    msgHeader.msg_iov = &msgBuffer;
    msgHeader.msg_iovlen = 1;
    msgHeader.msg_control = cmsgBuf;
    msgHeader.msg_controllen = CMSG_LEN(sizeof(fd));

    ssize_t bytesSent = recvmsg(socket, &msgHeader, 0);
    if (bytesSent < 0) {
        return -1;
    }

    struct cmsghdr *controlHeader = CMSG_FIRSTHDR(&msgHeader);
    memmove(&fd, CMSG_DATA(controlHeader), sizeof(int));
    return fd;
}

inline void initializeProcess(ze_context_handle_t &context,
                              ze_device_handle_t &device,
                              ze_command_queue_handle_t &cmdQueue,
                              ze_command_list_handle_t &cmdList,
                              bool isServer) {
    SUCCESS_OR_TERMINATE(zeInit(ZE_INIT_FLAG_GPU_ONLY));

    // Retrieve driver
    uint32_t driverCount = 0;
    SUCCESS_OR_TERMINATE(zeDriverGet(&driverCount, nullptr));

    ze_driver_handle_t driverHandle;
    SUCCESS_OR_TERMINATE(zeDriverGet(&driverCount, &driverHandle));

    ze_context_desc_t contextDesc = {};
    SUCCESS_OR_TERMINATE(zeContextCreate(driverHandle, &contextDesc, &context));

    // Retrieve device
    uint32_t deviceCount = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, nullptr));
    std::cout << "Number of devices found: " << deviceCount << "\n";

    std::vector<ze_device_handle_t> devices(deviceCount);
    SUCCESS_OR_TERMINATE(zeDeviceGet(driverHandle, &deviceCount, devices.data()));

    // Make the server use device0 and the client device1 if available
    if (deviceCount > 1) {
        ze_bool_t canAccessPeer = false;
        SUCCESS_OR_TERMINATE(zeDeviceCanAccessPeer(devices[0], devices[1], &canAccessPeer));
        if (canAccessPeer == false) {
            std::cerr << "Two devices found but no P2P capabilities detected\n";
            std::terminate();
        } else {
            std::cerr << "Two devices found and P2P capabilities detected\n";
        }
    }

    if (isServer) {
        device = devices[0];
        std::cout << "Server using device 0\n";
    } else {
        if (deviceCount > 1) {
            device = devices[1];
            std::cout << "Client using device 1\n";
        } else {
            device = devices[0];
            std::cout << "Client using device 0\n";
        }
    }

    // Print some properties
    ze_device_properties_t deviceProperties = {};
    SUCCESS_OR_TERMINATE(zeDeviceGetProperties(device, &deviceProperties));

    std::cout << "Device : \n"
              << " * name : " << deviceProperties.name << "\n"
              << " * vendorId : " << std::hex << deviceProperties.vendorId << "\n";

    // Create command queue
    uint32_t numQueueGroups = 0;
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups, nullptr));
    if (numQueueGroups == 0) {
        std::cerr << "No queue groups found!\n";
        std::terminate();
    }
    std::vector<ze_command_queue_group_properties_t> queueProperties(numQueueGroups);
    SUCCESS_OR_TERMINATE(zeDeviceGetCommandQueueGroupProperties(device, &numQueueGroups,
                                                                queueProperties.data()));

    ze_command_queue_desc_t cmdQueueDesc = {};
    for (uint32_t i = 0; i < numQueueGroups; i++) {
        if (queueProperties[i].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE) {
            cmdQueueDesc.ordinal = i;
        }
    }
    cmdQueueDesc.index = 0;
    cmdQueueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    SUCCESS_OR_TERMINATE(zeCommandQueueCreate(context, device, &cmdQueueDesc, &cmdQueue));

    // Create command list
    ze_command_list_desc_t cmdListDesc = {};
    cmdListDesc.commandQueueGroupOrdinal = cmdQueueDesc.ordinal;
    SUCCESS_OR_TERMINATE(zeCommandListCreate(context, device, &cmdListDesc, &cmdList));
}

void run_client(int commSocket) {
    std::cout << "Client process " << std::dec << getpid() << "\n";

    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initializeProcess(context, device, cmdQueue, cmdList, false);

    void *zeBuffer;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer, reinterpret_cast<void *>(&expectedPattern),
                                                       sizeof(expectedPattern), allocSize, nullptr, 0, nullptr));

    // get the dma_buf from the other process
    int dma_buf_fd = recvmsg_fd(commSocket);
    if (dma_buf_fd < 0) {
        std::cerr << "Failing to get dma_buf fd from server\n";
        std::terminate();
    }
    ze_ipc_mem_handle_t pIpcHandle;
    memcpy(&pIpcHandle, static_cast<void *>(&dma_buf_fd), sizeof(dma_buf_fd));

    // get a memory pointer to the BO associated with the dma_buf
    void *zeIpcBuffer;
    SUCCESS_OR_TERMINATE(zeMemOpenIpcHandle(context, device, pIpcHandle, 0u, &zeIpcBuffer));

    // Copy from client to server
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, zeIpcBuffer, zeBuffer, allocSize, nullptr, 0, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    SUCCESS_OR_TERMINATE(zeMemCloseIpcHandle(context, zeIpcBuffer));
    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));

    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));

    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

void run_server(int commSocket, bool &validRet) {
    std::cout << "Server process " << std::dec << getpid() << "\n";

    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t cmdQueue;
    ze_command_list_handle_t cmdList;
    initializeProcess(context, device, cmdQueue, cmdList, true);

    void *zeBuffer = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    SUCCESS_OR_TERMINATE(zeMemAllocDevice(context, &deviceDesc, allocSize, allocSize, device, &zeBuffer));

    // Initialize the IPC buffer
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, zeBuffer, reinterpret_cast<void *>(&uinitializedPattern),
                                                       sizeof(uinitializedPattern), allocSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));
    SUCCESS_OR_TERMINATE(zeCommandListReset(cmdList));

    // Get a dma_buf for the previously allocated pointer
    ze_ipc_mem_handle_t pIpcHandle;
    SUCCESS_OR_TERMINATE(zeMemGetIpcHandle(context, zeBuffer, &pIpcHandle));

    // Pass the dma_buf to the other process
    int dma_buf_fd;
    memcpy(static_cast<void *>(&dma_buf_fd), &pIpcHandle, sizeof(dma_buf_fd));
    if (sendmsg_fd(commSocket, static_cast<int>(dma_buf_fd)) < 0) {
        std::cerr << "Failing to send dma_buf fd to client\n";
        std::terminate();
    }

    char *heapBuffer = new char[allocSize];
    for (size_t i = 0; i < allocSize; ++i) {
        heapBuffer[i] = expectedPattern;
    }

    // Wait for child to exit
    int child_status;
    pid_t clientPId = wait(&child_status);
    if (clientPId <= 0) {
        std::cerr << "Client terminated abruptly with error code " << strerror(errno) << "\n";
        std::terminate();
    }

    void *validateBuffer = nullptr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    SUCCESS_OR_TERMINATE(zeMemAllocShared(context, &deviceDesc, &hostDesc, allocSize, 1, device, &validateBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryFill(cmdList, validateBuffer, reinterpret_cast<void *>(&uinitializedPattern),
                                                       sizeof(uinitializedPattern), allocSize, nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListAppendBarrier(cmdList, nullptr, 0, nullptr));

    // Copy from device-allocated memory
    SUCCESS_OR_TERMINATE(zeCommandListAppendMemoryCopy(cmdList, validateBuffer, zeBuffer, allocSize,
                                                       nullptr, 0, nullptr));

    SUCCESS_OR_TERMINATE(zeCommandListClose(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueExecuteCommandLists(cmdQueue, 1, &cmdList, nullptr));
    SUCCESS_OR_TERMINATE(zeCommandQueueSynchronize(cmdQueue, std::numeric_limits<uint64_t>::max()));

    // Validate stack and buffers have the original data from heapBuffer
    validRet = (0 == memcmp(heapBuffer, validateBuffer, allocSize));

    delete[] heapBuffer;
    SUCCESS_OR_TERMINATE(zeMemFree(context, zeBuffer));

    SUCCESS_OR_TERMINATE(zeCommandListDestroy(cmdList));
    SUCCESS_OR_TERMINATE(zeCommandQueueDestroy(cmdQueue));
    SUCCESS_OR_TERMINATE(zeContextDestroy(context));
}

int main(int argc, char *argv[]) {
    bool outputValidationSuccessful;

    int sv[2];
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        exit(1);
    }

    int child = fork();
    if (child < 0) {
        perror("fork");
        exit(1);
    } else if (0 == child) {
        close(sv[0]);
        run_client(sv[1]);
        close(sv[1]);
        exit(0);
    } else {
        close(sv[1]);
        run_server(sv[0], outputValidationSuccessful);
        close(sv[0]);
    }

    std::cout << "\nZello IPC P2P Results validation "
              << (outputValidationSuccessful ? "PASSED" : "FAILED")
              << std::endl;

    return 0;
}
