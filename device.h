#pragma once

#include <Wire.h>
#include "command.h"
#include <deque>

namespace CS {
    
    constexpr char versioning[] = "V1.1.0";
    constexpr int default_led_pin = 2;
    constexpr int default_port_sda = 5;
    constexpr int default_port_scl = 4;
    constexpr int default_baud_rate = 40000;
    constexpr uint32_t max_data_amount = 1 << 8;

    constexpr char tag_master[] = "[CS](M)";
    constexpr char tag_slave[]  = "[CS](S)";
    
    enum class device_id : uint8_t {
        DHT22_SENSOR,       /* Temperature and Humidity sensor */
        MICS_6814_SENSOR,  /* CO, NH3 and NO2 sensor */
        KY038_HW038_SENSOR,/* Loudness and lightness sensor */
        GY87_SENSOR,       /* Accelerometer, temperature, pressure, altitude and compass sensor */
        CCS811_SENSOR,       /* Quality of air sensor */
        PMSDS011_SENSOR,   /* Nova PM sensor */        
        BATTERY_SENSOR,    /* Own battery reporting sensor */
        _MAX               /* MAX to limit testing of devices */
    };

    void __handle_receive_slave(int);
    void __handle_request_slave();

    class DummyNCNM {
    public:
        DummyNCNM() = default;
        DummyNCNM(const DummyNCNM&) = delete;
        DummyNCNM(DummyNCNM&&) = delete;
        void operator=(const DummyNCNM&) = delete;
        void operator=(DummyNCNM&&) = delete;
    };
    
    class WireDevice : public DummyNCNM {
        HardwareSerial& m_log;
        const char* m_tag;
    protected:
        template<typename... Args>
        void logf(Args&&... a) { m_log.printf("%s ", m_tag); m_log.printf(a...); }
        void print_info(int sda, int scl, int led, int baud) {            
          m_log.printf(
            "[CS] ===============================\n"
            "[CS] # CustomSerial library \n"
            "[CS] - Version: %s\n"
            "[CS] - I2C baud speed: %i\n"
            "[CS] - Packages limit: %zu\n"
            "[CS] - Default ports (LED, SDA, SCL): %i, %i, %i\n"
            "[CS] ===============================\n", 
          versioning,
          baud,
          max_data_amount, 
          led, sda, scl);
        }
    public:
        WireDevice(HardwareSerial& hs, const bool is_master) : DummyNCNM(), m_log(hs), m_tag(is_master ? tag_master : tag_slave) {}
    };


    class MasterDevice : public WireDevice {
        std::deque<Data> m_data;
        bool m_map_available[static_cast<size_t>(device_id::_MAX)]{false};
        uint8_t m_id_requesting = 0;
    public:
        MasterDevice(HardwareSerial& hs, int sda = default_port_sda, int scl = default_port_scl, int baud = default_baud_rate)
            : WireDevice(hs, true)
        {
            print_info(sda, scl, -1, baud);
            Wire1.begin(sda, scl, baud);
            //Wire1.setWireTimeout(5000, false);
            logf("Begin!\n");
        }

        void check_devices_available() {
            logf("Checking devices available until at least one answers... \n");
            uint8_t devices_on = 0;
            do {
                for(uint8_t p = 0; p < static_cast<uint8_t>(device_id::_MAX); ++p) {
#ifdef _DEBUG
                    logf("[DEBUG] Step %hu of %hu, sending ping...\n", (uint16_t)p, static_cast<uint16_t>(device_id::_MAX));
#endif
                    Request req(false);
                    // set request offset
                    Wire1.beginTransmission(m_id_requesting);
                    Wire1.write((uint8_t*)&req, sizeof(req));
                    Wire1.endTransmission();
                    // request answer
                    Wire1.requestFrom(m_id_requesting, static_cast<size_t>(1), false);
                    
                    uint8_t test = Wire1.read();
                    if (test == 1) {
                        m_map_available[p] = true;
                        ++devices_on;
                    }
                }
            } while(devices_on == 0);
            logf("Done checking devices!\n");
        }

        size_t devices_available() {
            size_t c = 0;
            for(size_t p = 0; p < static_cast<size_t>(device_id::_MAX); ++p){
                if (m_map_available[p]) ++c;
            }
            return c;
        }
        
        // resets request to begin a new loop
        void begin_requests() {
            m_id_requesting = 0;
            m_data.resize(0);
        }

        // returns true while current request is < max id. False means the end of the loop
        bool request_next() {
            while(m_id_requesting < static_cast<uint8_t>(device_id::_MAX) && m_map_available[m_id_requesting] == false)
                ++m_id_requesting;

            if (m_id_requesting >= static_cast<uint8_t>(device_id::_MAX)) return false; // end of the loop

            for(uint32_t off = 0; off < max_data_amount; ++off) {
                Request req(off);
                // set request offset
                Wire1.beginTransmission(m_id_requesting);
                Wire1.write((uint8_t*)&req, sizeof(req));
                Wire1.endTransmission();
                // request answer
                Wire1.requestFrom(m_id_requesting, static_cast<size_t>(sizeof(Data)), false);

                Data dat(true);
                if (!dat.is_valid()) break;

                m_data.push_back(std::move(dat));
            }

            if (m_data.size() == 0) {
                m_map_available[m_id_requesting] = false;
                logf("Device ID %hu didn't return a thing and now is considered disconnected.\n", (uint16_t)m_id_requesting);
            }

            return true;
        }

        // used with request_next() in a loop. This will have the data, if any.
        const std::deque<Data>& get_data() const {
            return m_data;
        }

        uint8_t get_current() const {
            return m_id_requesting;
        }
    };


    class SlaveDevice : public WireDevice {
        friend void __handle_request_slave();
        friend void __handle_receive_slave(int);

        Request m_last_receive{false};
        void (*m_cb)(SlaveDevice&, const Request&); // must reply_with once!
        int m_led = -1;
        bool m_led_state = false;

        void _handle_request() {
            if (m_last_receive.is_ping()) {
                const uint8_t tmp = 1;
                Wire1.write(&tmp, 1);
            }
            else if (m_cb) {
                m_cb(*this, m_last_receive);
            } 
        }

        void _toggle_led() { if (m_led >= 0) digitalWrite(m_led, m_led_state = !m_led_state); }
    public:
        static void*& _get_singleton() { static void* v = nullptr; return v; }

        SlaveDevice(HardwareSerial& hs, device_id id, void(*req_cb)(SlaveDevice&, const Request&), int sda = default_port_sda, int scl = default_port_scl, int led = default_led_pin, int baud = default_baud_rate)
            : WireDevice(hs, false)
        {
            print_info(sda, scl, led, baud);

            if (_get_singleton() != 0) logf("Warn: redefinition of Slave. You should not re-create SlaveDevices! Continuing anyway.\n");
            _get_singleton() = (void*)this;
            
            if (led >= 0) pinMode(m_led = led, OUTPUT);

            Wire1.begin(static_cast<uint8_t>(id), sda, scl, baud);
            Wire1.onRequest(__handle_request_slave);
            Wire1.onReceive(__handle_receive_slave);
            logf("Begin!\n");
        }
        ~SlaveDevice() {
            _get_singleton() = nullptr;
            if (m_led >= 0) digitalWrite(m_led, false);
        }

        void reply_with(const Data& dat) {
            Wire1.write((uint8_t*)&dat, sizeof(dat));
        }
    };

    void __handle_request_slave()
    {
        SlaveDevice* sd = (SlaveDevice*)SlaveDevice::_get_singleton();
        if (!sd) return;
#ifdef _DEBUG
        sd->logf("__handle_request_slave got request, handling it...\n");
#endif
        sd->_handle_request();
#ifdef _DEBUG
        sd->logf("__handle_request_slave handled.\n");
#endif
    }

    // MUST NOT RESPOND
    void __handle_receive_slave(int length)
    {
        SlaveDevice* sd = (SlaveDevice*)SlaveDevice::_get_singleton();
        if (!sd) return;

#ifdef _DEBUG
        sd->logf("__handle_receive_slave Working on received data....\n");
#endif
        sd->_toggle_led();

        Request req(true);

        sd->m_last_receive = req;
        
#ifdef _DEBUG
        sd->logf("__handle_receive_slave Worked on received data. Stored. Ready.\n");
#endif
    }

}