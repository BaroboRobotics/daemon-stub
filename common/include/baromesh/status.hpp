#ifndef BAROMESH_STATUS_HPP
#define BAROMESH_STATUS_HPP

#include "daemon.pb.h"

namespace baromesh {

enum class Status {
    OK                      = barobo_Status_OK,
    CANNOT_OPEN_DONGLE      = barobo_Status_CANNOT_OPEN_DONGLE,
    DONGLE_NOT_FOUND        = barobo_Status_DONGLE_NOT_FOUND,
    PORT_OUT_OF_RANGE       = barobo_Status_PORT_OUT_OF_RANGE,
    NO_ROBOT_ENDPOINT       = barobo_Status_NO_ROBOT_ENDPOINT,

    UNREGISTERED_SERIALID   = barobo_Status_UNREGISTERED_SERIALID,
    INVALID_SERIALID        = barobo_Status_INVALID_SERIALID,

    DAEMON_UNAVAILABLE      = barobo_Status_DAEMON_UNAVAILABLE,

    STRANGE_DONGLE          = barobo_Status_STRANGE_DONGLE,
    RPC_VERSION_MISMATCH    = barobo_Status_RPC_VERSION_MISMATCH,

    BUFFER_OVERFLOW         = barobo_Status_BUFFER_OVERFLOW,
    OTHER_ERROR             = barobo_Status_OTHER_ERROR,
};

} // namespace baromesh

#endif