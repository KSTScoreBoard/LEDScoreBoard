/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */
#pragma once

#include <TWELITE>
#include "common.hpp"

class MY_APP_CHILD : MWX_APPDEFS_CRTP(MY_APP_CHILD)
{
public:
    static const uint8_t TYPE_ID = 0x02;

    // load common definition for handlers
    #define __MWX_APP_CLASS_NAME MY_APP_CHILD
    #include "_mwx_cbs_hpphead.hpp"
    #undef __MWX_APP_CLASS_NAME

private:

public:
    // constructor
    MY_APP_CHILD() :
        _buff_busy(0),
        _buff_available(0),
        _buff_bit_index(0),
        _buff{},
        _dio_mask(0),
        _dio_count(0), _dio_count_pad(0),
        _dio_list{}
    {
    }

    // begin method (if necessary, configure object here)
    void setup();

    // begin method (if necessary, start object here)
    void begin();

public: // syetem callback
    void on_create(uint32_t& val) { this->setup();  } // called when registered.
    void on_begin(uint32_t& val) { this->begin(); } // called when first call of ::loop()
    void on_message(uint32_t& val) { }

public:
    // TWENET callback handler
    // loop(), same with ::loop()
    void loop();

    // called when about to sleep.
    void on_sleep(uint32_t & val) {}

    // callback when waking up from sleep (very early stage, no init of peripherals.)
    void warmboot(uint32_t & val) {}

    // callback when waking up from sleep
    void wakeup(uint32_t & val) {}

public: // never called the following as hardware class, but define it!
    void receive(mwx::packet_rx& rx) {}
    void transmit_complete(mwx::packet_ev_tx& txev) {
        PEV_Process(E_ORDER_KICK, txev.u8CbId); // pass an event of tx completion to the state machine.
    }
    void network_event(mwx::packet_ev_nwk& pEvNwk) {}

// HERE, APP SPECIFIC DEFS, you can use any up to your design policy.
public:
    static const uint8_t STATE_IDLE = E_MWX::STATE_0;
    static const uint8_t STATE_TX = E_MWX::STATE_1;

private:
    static const int BUFF_AVAIL_0 = 0x100;
    static const int BUFF_AVAIL_1 = 0x101;
    static const int BUFF_AVAIL_NA = 0x000;
    static inline int _buff_get_avail_idx(int av) { return av == -1 ? -1 : (av - 0x100); }

    int _buff_busy;             // -1: not defined, 1:writing to _buff[0] 2:writing to _buff[1]
    int _buff_available;        // 0: n/a, 0x100: _buff[0] is ready to send, 0x101: _buff[0]
    int _buff_bit_index;
    buff_type _buff[2][N_BUFFER * sizeof(buff_type)];
    uint32_t _dio_mask;
    uint8_t _dio_count, _dio_count_pad;
    uint8_t _dio_list[sizeof(buff_type) * 8];

    void init_vars();

public:
    void start_capture(int hz, uint8_t dio_cout, uint8_t *dio_list);
    void stop_capture();
};
