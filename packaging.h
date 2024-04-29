#include "protocol.h"
#include <deque>

namespace CS {
    
    constexpr size_t max_path_len = 32;
    constexpr size_t max_requests = 50;
    
    class PackagedWired;
    
    class Requester {
        const size_t m_offset = 0;
    public:
        Requester() = default;
        Requester(const char* req_buf) { memcpy(get_as_data(), req_buf, get_as_data_len()); }
        Requester(const size_t off) : m_offset(off) {}
        Requester(const int off) : m_offset(static_cast<size_t>(off)) {}
        
        size_t get_offset() const { return m_offset; }
        
        char* get_as_data() { return (char*)this; }
        const char* get_as_data() const { return (char*)this; }
        size_t get_as_data_len() const { return sizeof(*this); }
    };
        
	struct Command {        
		enum class vtype : uint8_t { INVALID, TD, TF, TI, TU, REQUEST = std::numeric_limits<uint8_t>::max() };

		union _ {
			double d;
			float f;
			int64_t i;
			uint64_t u;

			_()            : d(0.0) {}
			_(double _d)   : d(_d)  {}
			_(float _f)    : f(_f)  {}
			_(int64_t _i)  : i(_i)  {}
			_(uint64_t _u) : u(_u)  {}
			void operator=(double _d)   { d = _d; }
			void operator=(float _f)    { f = _f; }
			void operator=(int64_t _i)  { i = _i; }
			void operator=(uint64_t _u) { u = _u; }
            
            operator double() const   { return d; }
            operator float() const    { return f; }
            operator int64_t() const  { return i; }
            operator uint64_t() const { return u; }
		};
	private:
		char m_path[max_path_len]{};
		_ m_val{0.0f};
		vtype m_type = vtype::INVALID;

		void _set_val(double _d)   { m_val = _d; m_type = vtype::TD; }
		void _set_val(float _f)    { m_val = _f; m_type = vtype::TF; }
		void _set_val(int64_t _i)  { m_val = _i; m_type = vtype::TI; }
		void _set_val(uint64_t _u) { m_val = _u; m_type = vtype::TU; }
        
	public:
        // as invalid
        Command() = default;
        
        Command(Command&& o) { memcpy(get_as_data(), o.get_as_data(), get_as_data_len()); }
        Command(const Command& o) { memcpy(get_as_data(), o.get_as_data(), get_as_data_len()); }
        void operator=(Command&& o) { memcpy(get_as_data(), o.get_as_data(), get_as_data_len()); }
        void operator=(const Command& o) { memcpy(get_as_data(), o.get_as_data(), get_as_data_len()); }
    
		template<typename T>
		Command(const char* path, const T& value)
		{
			const auto len = strlen(path);
			memcpy(m_path, path, (len + 1) > max_path_len ? max_path_len : len + 1);
			_set_val(value);
		}

        template<typename T>
        T           get_val() const         { return (T)m_val;}        
		const char* get_path() const        { return m_path;  }
		vtype       get_type() const        { return m_type;  }
		
		bool valid() const { 
            return 
                m_type == vtype::TD ||
                m_type == vtype::TF ||
                m_type == vtype::TI ||
                m_type == vtype::TU ||
                m_type == vtype::REQUEST;
        }
        
        
        char* get_as_data() { return (char*)this; }
        const char* get_as_data() const { return (char*)this; }
        size_t get_as_data_len() const { return sizeof(*this); }
	};
    
    class PackagedWired : public Wired {
    public:
        PackagedWired(const config& cfg) : Wired(cfg) {}
        
        Command master_do(device_id to, const Requester& req) {
            CS_LOGF("PackagedWired master_do about to request %zu from %hu\n", req.get_offset(), (uint16_t)d2u(to));
            Command cmd;
            this->Wired::master_do(to,
                req.get_as_data(), static_cast<uint8_t>(req.get_as_data_len()),
                cmd.get_as_data(), static_cast<uint8_t>(cmd.get_as_data_len())
            );
            return cmd; // if master_do fails, cmd will be invalid anyway. No need to check.
        }
        
        std::deque<Command> master_request_all(device_id to)
        {
            std::deque<Command> cmds;
            
            for(size_t p = 0; p < max_requests; ++p)
            {
                Requester req(p);
                Command cmd = master_do(to, req);
                if (!cmd.valid()) break;
                cmds.push_back(cmd);
            }
            
            return cmds;
        }
        
        void slave_reply_from_callback(const Command& cmd) {
            this->Wired::slave_reply_from_callback(cmd.get_as_data(), cmd.get_as_data_len());
        }
    };
}