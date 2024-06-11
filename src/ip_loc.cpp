#include <ip_loc.hpp>
#ifdef ESP_PLATFORM
#if __has_include(<Arduino.h>)
#include <Arduino.h>
#include <HTTPClient.h>
#else
#include <esp_http_client.h>
#endif
#include <json.hpp>
json::json_reader_ex<512> ip_loc_json_reader;
#ifdef ARDUINO
namespace arduino {
#else
namespace esp_idf {

class ip_loc_stream : public io::stream {
    char m_buffer[1024];
    size_t m_buffer_size;
    size_t m_buffer_start;
    esp_http_client_handle_t m_client;
    bool fill_buffer() {
        size_t data_read = esp_http_client_read(m_client, m_buffer, sizeof(m_buffer));
        if (data_read <= 0) {
            /*if (esp_http_client_is_complete_data_received(m_client)) {
                return false;
            } else {
                return false;
            }*/
            return false;
        }
        m_buffer_start = 0;
        m_buffer_size = data_read;
        return true;
    }
public:
    ip_loc_stream(esp_http_client_handle_t client) : m_buffer_size(0),m_buffer_start(0), m_client(client) {

    }
    virtual io::stream_caps caps() const override {
        io::stream_caps result;
        result.read = 1;
        result.seek = 0;
        result.write = 0;
        return result;
    }
    virtual int getch() override {
        if(m_buffer_start<m_buffer_size) {
            return m_buffer[m_buffer_start++];
        }
        if(!fill_buffer()) {
            return -1;
        }
        return m_buffer[m_buffer_start++];
    }
    virtual size_t read(uint8_t* data, size_t size) override {
        // Unexpected call to read() - not implemented
        // shouldn't be needed
        assert(false); // not implemented
        return 0;
    }
    virtual int putch(int value) override {
        return 0;
    }
    virtual size_t write(const uint8_t* data, size_t size) override {
        return 0;
    }
    virtual unsigned long long seek(long long position, io::seek_origin origin) override {
        return 0;
    }
};
#endif
static char* ip_loc_fetch_replace_char(char* str, char find, char replace){
    char *current_pos = strchr(str,find);
    while (current_pos) {
        *current_pos = replace;
        current_pos = strchr(current_pos+1,find);
    }
    return str;
}
bool ip_loc::fetch(float* out_lat,
                float* out_lon, 
                long* out_utc_offset, 
                char* out_region, 
                size_t region_size, 
                char* out_city, 
                size_t city_size,
                char* out_time_zone,
                size_t time_zone_size) {
    // URL for IP resolution service
    char url[256];
    *url = 0;
    strcpy(url,"http://ip-api.com/json/?fields=status");//,region,city,lat,lon,timezone,offset";
    int count = 0;
    if(out_lat!=nullptr) {
        *out_lat = 0.0f;
        strcat(url,",lat");
        ++count;
    }
    if(out_lon!=nullptr) {
        *out_lon = 0.0f;
        strcat(url,",lon");
        ++count;
    }
    if(out_utc_offset!=nullptr) {
        *out_utc_offset = 0;
        strcat(url,",offset");
        ++count;
    }
    if(out_region!=nullptr && region_size>0) {
        *out_region = 0;
        strcat(url,",region");
        ++count;
    }
    if(out_city!=nullptr && city_size>0) {
        *out_city = 0;
        strcat(url,",city");
        ++count;
    }
    if(out_time_zone!=nullptr && time_zone_size>0) {
        *out_time_zone = 0;
        strcat(url,",timezone");
        ++count;
    }
#ifdef ARDUINO
    HTTPClient client;
    client.begin(url);
    if(0>=client.GET()) {
        return false;
    }
    Stream& hstm = client.getStream();
    io::arduino_stream stm(&hstm);
#else
    esp_http_client_config_t cfg;
    memset(&cfg,0,sizeof(cfg));
    cfg.auth_type = HTTP_AUTH_TYPE_NONE;
    cfg.url = url;
    esp_http_client_handle_t client_handle = esp_http_client_init(&cfg);
    if(client_handle==nullptr) {
        return false;
    }
    if(ESP_OK!=esp_http_client_open(client_handle,0)) {
        esp_http_client_cleanup(client_handle);    
        return false;
    }
    esp_http_client_fetch_headers(client_handle);
    int sc = esp_http_client_get_status_code(client_handle);
    if(sc<200||sc>299) {
        esp_http_client_close(client_handle);
        esp_http_client_cleanup(client_handle);    
        return false;
    }
    
    ip_loc_stream stm(client_handle);
#endif
    ip_loc_json_reader.set(stm);
    while(ip_loc_json_reader.read()) {
        if(ip_loc_json_reader.depth()==1 && ip_loc_json_reader.node_type()==json::json_node_type::field) {
            if(out_lat!=nullptr && 0==strcmp("lat",ip_loc_json_reader.value())) {
                ip_loc_json_reader.read();
                *out_lat = ip_loc_json_reader.value_real();
                --count;
            } else if(out_lon!=nullptr && 0==strcmp("lon",ip_loc_json_reader.value())) {
                ip_loc_json_reader.read();
                *out_lon = ip_loc_json_reader.value_real();
                --count;
            } else if(out_utc_offset!=nullptr && 0==strcmp("offset",ip_loc_json_reader.value())) {
                ip_loc_json_reader.read();
                *out_utc_offset = ip_loc_json_reader.value_int();
                --count;
            } else if(out_region!=nullptr && region_size>0 && 0==strcmp("region",ip_loc_json_reader.value())) {
                ip_loc_json_reader.read();
                strncpy(out_region,ip_loc_json_reader.value(),region_size);
                --count;
            } else if(out_city!=nullptr && city_size > 0 && 0==strcmp("city",ip_loc_json_reader.value())) {
                ip_loc_json_reader.read();
                strncpy(out_city,ip_loc_json_reader.value(),city_size);
                --count;
            } else if(out_time_zone!=nullptr && time_zone_size>0 && 0==strcmp("timezone",ip_loc_json_reader.value())) {
                ip_loc_json_reader.read();
                strncpy(out_time_zone, ip_loc_json_reader.value(),time_zone_size);
                ip_loc_fetch_replace_char(out_time_zone,'_',' ');
                --count;
            }
        } else if(count<1 || ip_loc_json_reader.depth()==0) {
            // don't wait for end of document to terminate the connection
            break;
        }
    }
#ifdef ARDUINO
    client.end();
#else
    esp_http_client_close(client_handle);
    esp_http_client_cleanup(client_handle);
#endif
    return true;
}
}
#endif