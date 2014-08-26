#ifndef BAROMESH_DONGLETRANSPORT_HPP
#define BAROMESH_DONGLETRANSPORT_HPP

#include "dongledevicepath.hpp"

#define SFP_CONFIG_THREADSAFE
#include "sfp/context.hpp"
#include "serial/serial.h"

#include <exception>

namespace dongle {

constexpr const uint32_t kBaudRate = 230400;
const auto kSerialTimeout = serial::Timeout::simpleTimeout(200);
constexpr const auto kSfpSettleTimeout = std::chrono::milliseconds(200);
constexpr const auto kRetryCooldown = std::chrono::milliseconds(200);

struct Exception : std::exception { };

struct ThreadException : Exception {
    ThreadException (std::exception_ptr eptr)
            : underlyingExceptionPtr(eptr) { }

    virtual const char* what () const noexcept override {
        return "Unable to start reader thread";
    }

    std::exception_ptr underlyingExceptionPtr;
};

// Encapsulate serial::Serial and sfp::Context to create a reliable,
// message-oriented USB link.
class Transport {
public:


    Transport () {
        mSfpContext.output.connect(
                BIND_MEM_CB(&Transport::writeToUsb, this));
        mSfpContext.messageReceived.connect(
                BIND_MEM_CB(&MessageReceived::operator(), &messageReceived));
    }

    ~Transport () {
        mKillThread = true;
        mThread.join();
    }

    void startReaderThread () {
        try {
            std::thread t { &Transport::threadMain, this };
            mThread.swap(t);
        }
        catch (...) {
            throw ThreadException(std::current_exception());
        }
    }

    void sendMessage (const uint8_t* data, size_t size) {
        mSfpContext.sendMessage(data, size);
    }

    using MessageReceived = util::Signal<void(const uint8_t*,size_t)>;
    MessageReceived messageReceived;
    
    enum class DownReason {
        NORMALLY, EXCEPTIONALLY
    };

    util::Signal<void()> linkUp;
    util::Signal<void(DownReason)> linkDown;

private:
    void writeToUsb (uint8_t octet) {
        // FIXME: this is terribly inefficient to be doing for every single byte
        std::unique_lock<std::timed_mutex> lock {
            mUsbMutex,
            std::chrono::seconds(10)
        };
        if (!lock) {
            throw std::runtime_error("timed out waiting on USB lock");
        }
        if (mUsb) {
            mUsb->write(&octet, 1);
        }
        else {
            throw std::runtime_error("no dongle present");
        }
    }

    void threadMain () {
        while (!mKillThread) {
            if (threadInitialize()) {
                linkUp();
                try {
                    threadRun();
                    linkDown(DownReason::NORMALLY);
                }
                catch (...) {
                    linkDown(DownReason::EXCEPTIONALLY);
                }
            }
            if (!mKillThread) {
                std::this_thread::sleep_for(kRetryCooldown);
            }
        }
    }

    // True if initialization succeeded, false otherwise.
    bool threadInitialize () {
        std::cerr << "threadInitialize\n";
        // Get the dongle device path, i.e.: /dev/ttyACM0, \\.\COM3, etc.
        char path[64];
        auto status = devicePath(path, sizeof(path));
        if (-1 == status) {
            std::cerr << "threadInitialize: no dongle found\n";
            return false;
        }

        std::cerr << "threadInitialize: found dongle at " << path << "\n";
        try {
            threadConstructUsb(std::string(path));
            threadConnectSfp();
        }
        catch (...) {
            std::cout << "threadConstructUsb threw\n";
            // TODO Figure out what to do with the following cases:
            // perms?
            // read error
            // libsfp failure?
            // break, continue?
            return false;
        }
        return true;
    }

    void threadConstructUsb (std::string path) {
        std::cerr << "threadConstructUsb\n";
        std::unique_lock<std::timed_mutex> lock {
            mUsbMutex,
            std::chrono::seconds(10)
        };
        if (!lock) {
            throw std::runtime_error("timed out waiting on USB lock");
        }
        mUsb.reset(new serial::Serial(path, kBaudRate, kSerialTimeout));
    }

    void threadConnectSfp () {
        std::cout << "threadConnectSfp\n";
        assert(mUsb);
        mSfpContext.connect();

        uint8_t byte;
        // FIXME maybe: the way this is written means that kSerialTimeout is
        // also our sfp-connection timeout.
        while (!mKillThread && !mSfpContext.isConnected()) {
            auto bytesread = mUsb->read(&byte, 1);
            if (bytesread) {
                mSfpContext.input(byte);
            }
            else {
                throw std::runtime_error("libsfp connection failure");
            }
        }

        std::cout << "threadConnectSfp: we think we're connected\n";

        // Although we think we're connected, we don't know if the remote host
        // agrees yet. Wait a little bit for the dust to settle.
        using Clock = std::chrono::steady_clock;
        auto timeStop = Clock::now() + kSfpSettleTimeout;
        // FIXME if kSerialTimeout > kSfpSettleTimeout, mUsb->read() could
        // block longer than we want. Solution: temporarily adjust mUsb's
        // timeout?
        while (!mKillThread && Clock::now() < timeStop) {
            auto bytesread = mUsb->read(&byte, 1);
            if (bytesread) {
                mSfpContext.input(byte);
            }
        }

        std::cerr << "threadConnectSfp: settle timeout elapsed\n";
    }

    void threadRun () {
        std::cerr << "threadRun\n";
        assert(mUsb);
        while (!mKillThread) {
            uint8_t byte;
            // Block until kSerialTimeout milliseconds have elapsed.
            // TODO check mUsb.available() and read all the bytes available
            auto bytesread = mUsb->read(&byte, 1);
            if (bytesread) {
                mSfpContext.input(byte);
            }
        }
    }

    sfp::Context mSfpContext;
    std::unique_ptr<serial::Serial> mUsb;
    std::timed_mutex mUsbMutex;

    std::atomic<bool> mKillThread = { false };
    std::thread mThread;
};

} // namespace dongle

#endif
