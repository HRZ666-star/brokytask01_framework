#include "bsp/stm32/can/hal_can.h"

namespace bsp {

HalCan* HalCan::active_ = nullptr;

namespace {

Error map_hal_status(HAL_StatusTypeDef status) noexcept {
    switch (status) {
        case HAL_OK:
            return Error::Ok;
        case HAL_BUSY:
            return Error::Busy;
        case HAL_TIMEOUT:
            return Error::Timeout;
        case HAL_ERROR:
        default:
            return Error::Hardware;
    }
}

bool valid_frame(const CanFrame& frame) noexcept {
    if (frame.dlc > 8U) {
        return false;
    }
    if (frame.extended) {
        return frame.id <= 0x1FFFFFFFU;
    }
    return frame.id <= 0x7FFU;
}

}  // namespace

HalCan::HalCan(CAN_HandleTypeDef& handle) noexcept : handle_(handle) {
    if (active_ == nullptr) {
        active_ = this;
    }
}

Error HalCan::init(const CanConfig& config) {
    if (handle_.Instance == nullptr) {
        return Error::NotInitialized;
    }
    if (config.bitrate != 500000U) {
        /* The first board bring-up is fixed at 500 kbit/s. */
        return Error::NotSupported;
    }
    if (started_) {
        (void)stop();
    }
    if (receiving_) {
        (void)stopReceive();
    }

    /*
     * APB1 = 36 MHz.  36 MHz / 8 = 4.5 MHz; 4.5 MHz / (1 + 6 + 2) = 500 kHz.
     * Sample point = (1 + 6) / 9 = 77.8 %, the usual CAN recommendation.
     */
    handle_.Init.Prescaler = 8U;
    handle_.Init.Mode = (config.mode == CanMode::Loopback) ? CAN_MODE_LOOPBACK
                                                           : CAN_MODE_NORMAL;
    handle_.Init.SyncJumpWidth = CAN_SJW_1TQ;
    handle_.Init.TimeSeg1 = CAN_BS1_6TQ;
    handle_.Init.TimeSeg2 = CAN_BS2_2TQ;
    handle_.Init.TimeTriggeredMode = DISABLE;
    handle_.Init.AutoBusOff = ENABLE;
    handle_.Init.AutoWakeUp = DISABLE;
    handle_.Init.AutoRetransmission = ENABLE;
    handle_.Init.ReceiveFifoLocked = DISABLE;
    handle_.Init.TransmitFifoPriority = DISABLE;

    if (HAL_CAN_Init(&handle_) != HAL_OK) {
        return Error::Hardware;
    }

    /* One 32-bit mask filter, bank 0, accept everything into FIFO 0. */
    CAN_FilterTypeDef filter = {};
    filter.FilterBank = 0U;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = 0U;
    filter.FilterIdLow = 0U;
    filter.FilterMaskIdHigh = 0U;
    filter.FilterMaskIdLow = 0U;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14U;
    if (HAL_CAN_ConfigFilter(&handle_, &filter) != HAL_OK) {
        return Error::Hardware;
    }

    mode_ = config.mode;
    initialized_ = true;
    started_ = false;
    return Error::Ok;
}

Error HalCan::start() {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (started_) {
        return Error::Ok;
    }
    if (HAL_CAN_Start(&handle_) != HAL_OK) {
        return Error::Hardware;
    }
    started_ = true;
    return Error::Ok;
}

Error HalCan::stop() {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (receiving_) {
        (void)stopReceive();
    }
    if (started_) {
        if (HAL_CAN_Stop(&handle_) != HAL_OK) {
            return Error::Hardware;
        }
        started_ = false;
    }
    return Error::Ok;
}

Error HalCan::send(const CanFrame& frame, std::uint32_t timeout_ms) {
    if (!initialized_ || !started_) {
        return Error::NotInitialized;
    }
    if (!valid_frame(frame)) {
        return Error::InvalidArgument;
    }

    CAN_TxHeaderTypeDef header = {};
    header.StdId = frame.extended ? 0U : frame.id;
    header.ExtId = frame.extended ? frame.id : 0U;
    header.IDE = frame.extended ? CAN_ID_EXT : CAN_ID_STD;
    header.RTR = frame.remote ? CAN_RTR_REMOTE : CAN_RTR_DATA;
    header.DLC = frame.dlc;
    header.TransmitGlobalTime = DISABLE;

    const std::uint32_t start_ms = HAL_GetTick();
    while (HAL_CAN_GetTxMailboxesFreeLevel(&handle_) == 0U) {
        if (timeout_ms == 0U) {
            return Error::Busy;
        }
        if ((HAL_GetTick() - start_ms) >= timeout_ms) {
            return Error::Timeout;
        }
        HAL_Delay(1U);
    }

    std::uint32_t mailbox = 0U;
    const HAL_StatusTypeDef status = HAL_CAN_AddTxMessage(
        &handle_, &header, const_cast<std::uint8_t*>(frame.data), &mailbox);
    return map_hal_status(status);
}

Error HalCan::receive(CanFrame& frame, std::uint32_t timeout_ms) {
    if (!initialized_ || !started_) {
        return Error::NotInitialized;
    }

    const std::uint32_t start_ms = HAL_GetTick();
    for (;;) {
        if (HAL_CAN_GetRxFifoFillLevel(&handle_, CAN_RX_FIFO0) > 0U) {
            CAN_RxHeaderTypeDef header = {};
            std::uint8_t data[8] = {};
            if (HAL_CAN_GetRxMessage(&handle_, CAN_RX_FIFO0, &header, data) !=
                HAL_OK) {
                return Error::Hardware;
            }

            frame.id = (header.IDE == CAN_ID_EXT) ? header.ExtId : header.StdId;
            frame.extended = (header.IDE == CAN_ID_EXT);
            frame.remote = (header.RTR == CAN_RTR_REMOTE);
            frame.dlc = static_cast<std::uint8_t>(header.DLC > 8U ? 8U : header.DLC);
            for (std::uint32_t i = 0U; i < 8U; ++i) {
                frame.data[i] = data[i];
            }
            return Error::Ok;
        }

        if (timeout_ms == 0U) {
            return Error::Empty;
        }
        if ((HAL_GetTick() - start_ms) >= timeout_ms) {
            return Error::Timeout;
        }
        HAL_Delay(1U);
    }
}

Error HalCan::startReceive(CanRxCallback callback, void* context) {
    if (!initialized_ || !started_) {
        return Error::NotInitialized;
    }
    if (callback == nullptr) {
        return Error::InvalidArgument;
    }
    if (receiving_) {
        return Error::Busy;
    }

    callback_ = callback;
    context_ = context;
    if (HAL_CAN_ActivateNotification(&handle_,
                                     CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        callback_ = nullptr;
        context_ = nullptr;
        return Error::Hardware;
    }
    receiving_ = true;
    return Error::Ok;
}

Error HalCan::stopReceive() {
    if (!initialized_) {
        return Error::NotInitialized;
    }
    if (!receiving_) {
        return Error::Ok;
    }
    if (HAL_CAN_DeactivateNotification(&handle_,
                                       CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        return Error::Hardware;
    }
    receiving_ = false;
    callback_ = nullptr;
    context_ = nullptr;
    return Error::Ok;
}

CanMode HalCan::mode() const noexcept { return mode_; }

void HalCan::on_rx_pending(CAN_HandleTypeDef* handle) {
    HalCan* self = active_;
    if (self == nullptr || &self->handle_ != handle || !self->receiving_) {
        return;
    }

    while (self->receiving_ &&
           HAL_CAN_GetRxFifoFillLevel(handle, CAN_RX_FIFO0) > 0U) {
        CAN_RxHeaderTypeDef header = {};
        std::uint8_t data[8] = {};
        if (HAL_CAN_GetRxMessage(handle, CAN_RX_FIFO0, &header, data) !=
            HAL_OK) {
            break;
        }

        CanFrame frame = {};
        frame.id = (header.IDE == CAN_ID_EXT) ? header.ExtId : header.StdId;
        frame.extended = (header.IDE == CAN_ID_EXT);
        frame.remote = (header.RTR == CAN_RTR_REMOTE);
        frame.dlc = static_cast<std::uint8_t>(header.DLC > 8U ? 8U : header.DLC);
        for (std::uint32_t i = 0U; i < 8U; ++i) {
            frame.data[i] = data[i];
        }

        if (self->callback_ != nullptr) {
            self->callback_(self->context_, frame);
        }
    }
}

}  // namespace bsp

extern "C" void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef* hcan) {
    bsp::HalCan::on_rx_pending(hcan);
}
