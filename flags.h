#pragma once

#include <stdint.h>

namespace CS {
    
    enum class device_flags : uint64_t {
        NONE =          0,
        HAS_ISSUES =    1 << 0,
        HAS_NEW_DATA =  1 << 1
    };
        
    constexpr uint64_t df2u(device_flags id) { return static_cast<uint64_t>(id); }
    
    class FlagWrapper {
        uint64_t m_flagged = 0;
    public:
        FlagWrapper() = default;
        FlagWrapper(const uint64_t v) : m_flagged(v) {}
        
        FlagWrapper& operator|=(const device_flags& df) { m_flagged |= df2u(df); return *this; }
        FlagWrapper& operator&=(const device_flags& df) { m_flagged &= df2u(df); return *this; }
        
        FlagWrapper operator|(const device_flags& df) { return { m_flagged | df2u(df) }; }
        FlagWrapper operator&(const device_flags& df) { return { m_flagged & df2u(df) }; }
        
        operator uint64_t() const { return m_flagged; }
        
        bool has(const device_flags& df) const  { return (m_flagged & df2u(df)) > 0; }
        void set(const device_flags& df)        { m_flagged |= df2u(df); }
        void remove(const device_flags& df)     { m_flagged &= ~df2u(df); }
    };
}