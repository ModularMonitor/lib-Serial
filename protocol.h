#include <Arduino.h>
#include <Wire.h>

#ifdef _DEBUG
#define CS_LOGF(...) Serial.print("[CS][Debug] "); Serial.printf(__VA_ARGS__);
#else
#define CS_LOGF(...) {}
#endif

namespace CS {
    
    constexpr char version[] = "V1.1.0b";
    constexpr size_t max_length_wire = 255; // to fit uint8_t
    
    enum class device_id : uint8_t {
        DHT22_SENSOR,       /* Temperature and Humidity sensor */
        MICS_6814_SENSOR,   /* CO, NH3 and NO2 sensor */
        KY038_HW038_SENSOR, /* Loudness and lightness sensor */
        GY87_SENSOR,        /* Accelerometer, temperature, pressure, altitude and compass sensor */
        CCS811_SENSOR,      /* Quality of air sensor */
        PMSDS011_SENSOR,    /* Nova PM sensor */        
        BATTERY_SENSOR,     /* Own battery reporting sensor */
        _MAX                /* MAX to limit testing of devices */
    };
    
    constexpr uint8_t d2u(device_id id) { return static_cast<uint8_t>(id); }
    const char* d2str(device_id id) {
        switch(id) {
        case device_id::DHT22_SENSOR:       return "DHT22_SENSOR";
        case device_id::MICS_6814_SENSOR:   return "MICS_6814_SENSOR";
        case device_id::KY038_HW038_SENSOR: return "KY038_HW038_SENSOR";
        case device_id::GY87_SENSOR:        return "GY87_SENSOR";
        case device_id::CCS811_SENSOR:      return "CCS811_SENSOR";
        case device_id::PMSDS011_SENSOR:    return "PMSDS011_SENSOR";
        case device_id::BATTERY_SENSOR:     return "BATTERY_SENSOR";
        default:                            return "UNKNOWN";
        }
    }
    
    void __wired_receive_handler(int received_bytes);
    //void __wired_request_handler();
        
    struct config {
        // self, wants, got, got_len
        void (*callback_event)(void*, const uint8_t, const char*, const uint8_t) = nullptr;
        int sda = 5;
        int scl = 4;
        int led = -1;
        int baud = 40000;
        uint8_t slave_id = 0;
        bool master = false;
        
        config& set_slave_callback(void (*cb)(void*, const uint8_t, const char*, const uint8_t)) { callback_event = cb; return *this; }
        config& set_sda(int v) { sda = v; return *this; }
        config& set_scl(int v) { scl = v; return *this; }
        config& set_led(int v) { led = v; return *this; }
        config& set_baud(int v) { baud = v; return *this; }
        config& set_master() { slave_id = 0; master = true; return *this; }
        config& set_slave(device_id id) { slave_id = d2u(id); master = false; return *this; }
    };
    
    class Wired {
    public:
        static Wired*& get_singleton() { static Wired* p = nullptr; return p;}
    private:
        struct master_model {
            // BEGIN OF PACKAGE SENT (do not change) {
            uint8_t expect_reply_len = 0;
            char raw_data[max_length_wire - sizeof(uint8_t)]{};
            // } END OF PACKAGE SENT
            uint8_t len = 0; // package raw_data len
        };
    
        friend void __wired_receive_handler(int);
        //friend void __wired_request_handler();
    
        master_model m_buffer;
        const config m_cfg;
        bool m_led_last_state = false;
        
        void _led(bool state){ if (m_cfg.led >= 0) digitalWrite(m_cfg.led, m_led_last_state = state); }
        void toggle_led() { _led(!m_led_last_state); }
        bool set_singleton() { if (get_singleton() != nullptr) return false; get_singleton() = this; return true; }
        void reset_singleton() { get_singleton() = nullptr; }
        
        // = = = LOGIC PART = = = //
        void _write(const char* data, const uint8_t len) {
            CS_LOGF("__ write len=%hu\n", (uint16_t)len);
            Wire1.write((uint8_t*)data, len);
        }
        
        void _slavewrite(const char* data, const uint8_t len) {
            CS_LOGF("__ slaveWrite len=%hu\n", (uint16_t)len);
            Wire1.slaveWrite((uint8_t*)data, len);
        }
        
        bool _read(char* data, const uint8_t expected) {
            CS_LOGF("__ read len=%hu\n", (uint16_t)expected);
            if (Wire1.available() < (int)expected) {
                CS_LOGF("__ read fail, got %i, expected %hu\n", (int)Wire1.available(), (uint16_t)expected);
                return false;
            }
            Wire1.readBytes((uint8_t*)data, (int)expected);
            return true;
        }
        
        bool _master_send(device_id to, const char* data, const uint8_t len) {
            _led(true);
            CS_LOGF("_ master send to=%hu len=%hu ...", (uint16_t)d2u(to), (uint16_t)len);
            Wire1.beginTransmission(d2u(to));
            _write(data, len);
            const int res = Wire1.endTransmission(true);
            CS_LOGF("result %i\n", res);
            _led(false);
            return res == 0;
        }
        
        bool _master_request_and_read_from(device_id from, char* data, const uint8_t len) {
            _led(true);
            CS_LOGF("_ master req read from=%hu len=%hu\n", (uint16_t)d2u(from), (uint16_t)len);
            if (Wire1.requestFrom(d2u(from), (size_t)len, true) != (int)len) return false;
            delay(1);
            _led(false);
            return _read(data, len);
        }
        
        void _slave_send(const char* data, const uint8_t len) {
            CS_LOGF("_ slave send from=%hu len=%hu\n", (uint16_t)m_cfg.slave_id, (uint16_t)len);
            _write(data, len);
        }
        // = = = ENDOF LOGIC PART = = = //
        
        void _slave_internal_store_auto(const int event_got)
        {
            if (event_got <= 0) return; // nothing to do
            if (event_got > max_length_wire) {
                CS_LOGF("UNEXPECTED WIRE BUFFER LENGTH: GREATER THAN MAX. Copying only %zu.\n", max_length_wire);
            }
            m_buffer.len = static_cast<uint8_t>(static_cast<size_t>(event_got) > max_length_wire ? max_length_wire : static_cast<size_t>(event_got));
            
            _read((char*)&m_buffer, m_buffer.len);
            m_buffer.len -= sizeof(m_buffer.expect_reply_len); // remove size from total len
        }
        
        void _slave_internal_triggered_request()
        {
            if (!m_cfg.callback_event) {
                CS_LOGF("UNEXPECTED WIRE CALLBACK NOT SET ON SLAVE. Ignoring event. Good luck fixing this.\n");
                return;
            }
            
            m_cfg.callback_event(
                (void*)this,
                m_buffer.expect_reply_len,
                m_buffer.raw_data,
                m_buffer.len
            );
            
            m_buffer.len = 0; // reset
            m_buffer.expect_reply_len = 0; // reset
        }
    public:    
        Wired(const config& cfg) : m_cfg(cfg) {            
            if (!set_singleton()) {
                CS_LOGF("Fatal error: there are more than one Wired set up! You must not have more than one configured! Locking execution.\n");
                while(1) delay(100);
            }
            
            CS_LOGF("Setting up Wired...\n");
            CS_LOGF("SDA=%i; SCL=%i; LED=%i; BAUD=%i; MASTER=%c, SLAVE=%hu\n",
                m_cfg.sda, m_cfg.scl, m_cfg.led, m_cfg.baud, m_cfg.master ? 'Y' : 'N', static_cast<uint16_t>(m_cfg.slave_id));
            
            if (m_cfg.led >= 0) pinMode(m_cfg.led, OUTPUT);
            _led(true);
            
            if (m_cfg.master) {
                Wire1.begin(m_cfg.sda, m_cfg.scl, m_cfg.baud);
            }
            else {
                //Wire1.onRequest(__wired_request_handler);
                Wire1.onReceive(__wired_receive_handler);
                Wire1.begin(m_cfg.slave_id, m_cfg.sda, m_cfg.scl, m_cfg.baud);
            }
        }
        
        virtual ~Wired()
        {
            _led(false);
            reset_singleton();
        }
        
        bool master_do(device_id to, const char* data, const uint8_t data_len, char* recd, const uint8_t recd_expect)
        {
            if (data_len > sizeof(m_buffer.raw_data)) return false;
            
            m_buffer.expect_reply_len = recd_expect;
            memcpy(m_buffer.raw_data, data, data_len);
            m_buffer.len = data_len;
            
            // send if something to send
            if (data && data_len > 0) {
                if (!_master_send(to,(char*)&m_buffer, m_buffer.len + 1)) {
                    CS_LOGF("_master_send on master_do failed\n");
                    return false;
                }
            }
            //_master_flush();
            delay(1);
            // read if buffer to recv
            if (recd && recd_expect > 0) {
                if (!_master_request_and_read_from(to, recd, recd_expect)) return false;
            }
            
            return true;
        }
        
        void slave_reply_from_callback(const char* data, const uint8_t data_len)
        {
            // send if something to send
            if (data && data_len > 0) _slavewrite(data, data_len);
        }
        
        Wired(const Wired&) = delete;
        Wired(Wired&&) = delete;
        void operator=(const Wired&) = delete;
        void operator=(Wired&&) = delete;
    };
    
    
    inline void __wired_receive_handler(int received_bytes)
    {
        Wired* wired = Wired::get_singleton();
        CS_LOGF("Received %i bytes...\n", received_bytes);
        wired->_slave_internal_store_auto(received_bytes);
        wired->_slave_internal_triggered_request();
        wired->toggle_led();
    }
    
}