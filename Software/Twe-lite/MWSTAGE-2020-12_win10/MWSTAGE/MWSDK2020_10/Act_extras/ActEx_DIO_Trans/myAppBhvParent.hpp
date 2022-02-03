/* Copyright (C) 2020 Mono Wireless Inc. All Rights Reserved.    *
 * Released under MW-SLA-*J,*E (MONO WIRELESS SOFTWARE LICENSE   *
 * AGREEMENT).                                                   */
#pragma once

#include <TWELITE>
#include "common.hpp"

class MY_APP_PARENT : MWX_APPDEFS_CRTP(MY_APP_PARENT)
{
public:
    static const uint8_t TYPE_ID = 0x01;

    // load common definition for handlers
    #define __MWX_APP_CLASS_NAME MY_APP_PARENT
    #include "_mwx_cbs_hpphead.hpp"
    #undef __MWX_APP_CLASS_NAME

public:
    // constructor
    MY_APP_PARENT() {}

    // begin method (if necessary, configure object here)
    void setup();

    // begin method (if necessary, start object here)
    void begin();

public:
    // TWENET callback handler (mandate)
    void loop();

    void on_sleep(uint32_t & val) {
    }

    void warmboot(uint32_t & val) {
    }

    void wakeup(uint32_t & val) {
    }

    void on_create(uint32_t& val) { this->setup();  }
    void on_begin(uint32_t& val) { this->begin(); }
    void on_message(uint32_t& val) {}

public: // never called the following as hardware class, but define it!
    void network_event(mwx::packet_ev_nwk& pEvNwk) {}
    void receive(mwx::packet_rx& rx);
    void transmit_complete(mwx::packet_ev_tx& pEvTx) {}

// HERE, APP SPECIFIC DEFS, you can use any up to your design policy.
private:
    static const int N_BUFFER_BANK = 4; // number of buffer banks

    int _buff_head;        // -1: stopped
    int _buff_tail;        // head==tail (no more buffer)
    int _buff_bit_index;
    buff_type _buff[N_BUFFER_BANK][N_BUFFER * sizeof(buff_type)];
    uint8_t _dio_count, _dio_count_pad;
    uint8_t _dio_list[sizeof(buff_type) * 8];
    bool _monitor_active;
    int _seq_last;

    void init_vars();

public:
    void start_monitor(int hz, uint8_t dio_count, const uint8_t *dio_list);
    void stop_monitor();
};
