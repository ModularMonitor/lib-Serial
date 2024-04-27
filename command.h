#pragma once

#include <Wire.h>
#include <stdint.h>
#include <string.h>

namespace CS {

    constexpr int command_path_max_length = 16;
    constexpr uint8_t data_mask_flag_off = 0b01111111;
    constexpr uint8_t data_mask_flag     = 0b10000000;
    
    // uses 7 bits, 8th is used as flag
    enum class vtype : uint8_t { UNKNOWN = 0, F, I, U, _MAX, _FLAG_HAS_MORE = data_mask_flag };

    // This is used by SLAVES
    struct Data {
        // related to m_raw_data, to store float and int32 easy
        static_assert(sizeof(float) == 4, "Float is not of size 4 (bytes), so things will break");

        char m_path[command_path_max_length]{};
        char m_raw_data[sizeof(float)]{};
        uint8_t m_type_flag = static_cast<uint8_t>(vtype::UNKNOWN);

        uint8_t _get_type() const { return m_type_flag & data_mask_flag_off; }

        Data(const bool self_read_from_wire1 = false) { 
            memset(this, 0, sizeof(*this)); 
            if (self_read_from_wire1) {
                if (Wire1.available() != sizeof(Data)) return;
                Wire1.readBytes((uint8_t*)this, sizeof(Data));
            }
        }

        Data(const char* path, const uint32_t val, bool has_more = false) : m_type_flag(static_cast<uint8_t>(vtype::U)) {
            const auto plen = strlen(path);
            memcpy(m_path, path, plen > command_path_max_length ? command_path_max_length : plen);
            memcpy(m_raw_data, (char*)(&val), sizeof(m_raw_data));
            if (has_more) m_type_flag |= data_mask_flag;
        }
        Data(const char* path, const int32_t val, bool has_more = false) : m_type_flag(static_cast<uint8_t>(vtype::I)) {
            const auto plen = strlen(path);
            memcpy(m_path, path, plen > command_path_max_length ? command_path_max_length : plen);
            memcpy(m_raw_data, (char*)(&val), sizeof(m_raw_data));
            if (has_more) m_type_flag |= data_mask_flag;
        }
        Data(const char* path, const float val, bool has_more = false) : m_type_flag(static_cast<uint8_t>(vtype::F)) {
            const auto plen = strlen(path);
            memcpy(m_path, path, plen > command_path_max_length ? command_path_max_length : plen);
            memcpy(m_raw_data, (char*)(&val), sizeof(m_raw_data));
            if (has_more) m_type_flag |= data_mask_flag;
        }        

        Data(const Data&) = delete;
        Data(Data&& o) { memcpy(this, &o, sizeof(*this)); }
        void operator=(const Data&) = delete;
        void operator=(Data&& o) { memcpy(this, &o, sizeof(*this)); }

        const char* get_path() const { return m_path; }
        vtype get_type() const { return static_cast<vtype>(_get_type()); }
        bool has_more() const { return m_type_flag & data_mask_flag; }

        bool is_unsigned() const { return get_type() == vtype::U; }
        bool is_integer() const { return get_type() == vtype::I; }
        bool is_float() const { return get_type() == vtype::F; }

        bool is_valid() const { 
            return _get_type() <  static_cast<uint8_t>(vtype::_MAX) &&
                   _get_type() != static_cast<uint8_t>(vtype::UNKNOWN);
        }

        float get_as_float() const { return *((float*)m_raw_data); }
        int32_t get_as_integer() const { return *((int32_t*)m_raw_data); }
        uint32_t get_as_unsigned() const { return *((uint32_t*)m_raw_data); }
    };

    // This is used by MASTER
    struct Request {
        uint32_t m_request_offset = 0xFFFFFFFF;

        Request(const bool self_read_from_wire1)
        {
            if (self_read_from_wire1) {                
                if (Wire1.available() != sizeof(Request)) return;
                Wire1.readBytes((uint8_t*)this, sizeof(Request));
            }
        }
        Request(const uint32_t off) : m_request_offset(off)
        {
        }

        uint32_t get_offset() const { return m_request_offset; }
        bool is_ping() const { return m_request_offset == 0xFFFFFFFF; }
    };
    
}