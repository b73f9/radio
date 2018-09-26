#include <algorithm>

std::atomic_flag io_mutex = ATOMIC_FLAG_INIT;

void lck(std::atomic_flag &f){
    while (f.test_and_set(std::memory_order_acquire));
}

void ulck(std::atomic_flag &f){
    f.clear(std::memory_order_release);
}

uint64_t idStringToInt64(std::string str){
    uint64_t value;
    // We need this try...catch because stoull can throw exceptions on its own
    try {
        if(str.empty() || str[0] > '9' || str[0] < '0')
            throw std::invalid_argument("Id must be a positive number");
        value = stoull(str);
    }catch(std::exception &e){
        lck(io_mutex);
        std::cerr << "Error (" << e.what() << ") while parsing number: " << str << "\n";
        ulck(io_mutex);
        return UINT64_MAX;
    }

    return value;
}

std::vector<uint64_t> split_ids(const std::vector<char> data, const char delimeter){
    std::vector<uint64_t> result;

    auto found_pos = data.begin();
    auto prev_pos = data.begin();

    do{
        found_pos = std::find(found_pos, data.end(), delimeter);

        uint64_t value = idStringToInt64(std::string(prev_pos, found_pos));
        if(value != UINT64_MAX)
            result.push_back(value);

        if(found_pos != data.end())
            found_pos += 1; // ignore the delimeter
        prev_pos = found_pos;

    } while(found_pos != data.end());

    return result;
}
